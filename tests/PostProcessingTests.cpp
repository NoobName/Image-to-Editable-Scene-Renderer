#include "Renderer/PostProcessingPipeline.h"
#include "App/LookOptions.h"
#include "GpuImageReadback.h"
#include <algorithm>
#include <iostream>
#include <limits>
using namespace isr;
using namespace DirectX;
namespace {
unsigned checks=0;
void Require(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
bool Near(float a,float b,float epsilon=0.0001f){return std::isfinite(a)&&std::abs(a-b)<=epsilon*std::max(1.0f,std::abs(b));}
template<class F>void Reject(F action){bool rejected=false;try{action();}catch(const std::exception&){rejected=true;}Require(rejected,"Invalid look parameters accepted");}
void ParameterTests(){
    LookParameters look;ValidateLookParameters(look);
    for(const auto& p:LookParameterSchema){
        auto value=look;value.*(p.member)=p.minimum;ValidateLookParameters(value);value.*(p.member)=p.maximum;ValidateLookParameters(value);
        value.*(p.member)=std::numeric_limits<float>::quiet_NaN();Reject([&]{ValidateLookParameters(value);});
        value.*(p.member)=p.maximum+1;Reject([&]{ValidateLookParameters(value);});
    }
    Require(SetLookParameter(look,"exposure",2)&&look.exposure==2,"Shared parameter assignment failed");
    Reject([&]{SetLookParameter(look,"contrast",0);});Require(look.contrast==1,"Rejected edit changed state");
    Require(!SetLookParameter(look,"fog-density",0.5f),"Reserved parameter exposed as an active effect");
    auto parse=[&](std::wstring key,std::wstring value){wchar_t* argv[]={key.data(),value.data()};int index=0;return ParseLookOption(key,index,2,argv,look);};
    Require(parse(L"--temperature",L"0.5")&&look.temperature==0.5f,"CLI does not update shared LookParameters");
    Reject([&]{parse(L"--gamma",L"nan");});Reject([&]{parse(L"--exposure",L"2junk");});Reject([&]{parse(L"--saturation",L"3");});
    Require(parse(L"--tone-mapping",L"reinhard")&&look.toneMapping==ToneMapping::Reinhard,"Tone mapping CLI");
}
class Fixture {
public:
    DeviceContext context{true};
    // Exactly 12 descriptors: 10 bloom, one grade, one input. Resizes must not leak slots.
    DescriptorAllocator resources{context.Device(),D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,12,true};
    DescriptorAllocator rtv{context.Device(),D3D12_DESCRIPTOR_HEAP_TYPE_RTV,1};
    PostProcessingPipeline pipeline{context.Device(),resources};
    PostProcessTarget input{rtv,resources};
    ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12GraphicsCommandList> list;
    Fixture(){
        Check(context.Device()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator)));
        Check(context.Device()->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&list)));Check(list->Close());
    }
    std::vector<XMFLOAT4> Render(UINT width,UINT height,const LookParameters& look,XMFLOAT4 color,RenderMode mode=RenderMode::Final,bool impulse=false){
        context.Flush();pipeline.Resize(context.Device(),width,height);input.Resize(context.Device(),width,height);
        Check(allocator->Reset());Check(list->Reset(allocator.Get(),nullptr));
        ID3D12DescriptorHeap* heaps[]={resources.Heap()};list->SetDescriptorHeaps(1,heaps);
        input.Begin(list.Get());list->ClearRenderTargetView(input.Rtv(),&color.x,0,nullptr);
        if(impulse){const float bright[]={64,64,64,1};const D3D12_RECT rect{LONG(width/2),LONG(height/2),LONG(width/2+1),LONG(height/2+1)};
            list->ClearRenderTargetView(input.Rtv(),bright,1,&rect);}
        input.End(list.Get());
        const auto result=pipeline.ProcessHDR(list.Get(),input.Image(),look,mode);
        Check(list->Close());ID3D12CommandList* lists[]={list.Get()};context.Queue()->ExecuteCommandLists(1,lists);context.Flush();
        auto pixels=ReadGpuImage(context,result.texture->Resource())[0];
        for(const auto& p:pixels)Require(std::isfinite(p.x+p.y+p.z+p.w)&&p.x>=0&&p.y>=0&&p.z>=0,"Nonfinite or negative post output");
        return pixels;
    }
};
void GpuTests(){
    Fixture f;LookParameters look;auto pixels=f.Render(17,9,look,{2,0.25f,0.05f,0.7f});
    for(const auto& p:pixels)Require(p.x==2&&p.y==0.25f&&p.z==0.05f&&p.w==0.7f,"Neutral look is not identity");
    look.exposure=2;pixels=f.Render(17,9,look,{2,0.25f,0.05f,0.7f});
    for(const auto& p:pixels)Require(Near(p.x,8)&&Near(p.y,1)&&Near(p.z,0.2f)&&p.w==0.7f,"Exposure or alpha preservation failed");
    look={};look.saturation=0;pixels=f.Render(17,9,look,{1,0.2f,0.1f,1});
    const float luminance=0.2126f+0.7152f*0.2f+0.0722f*0.1f;
    for(const auto& p:pixels)Require(Near(p.x,luminance)&&Near(p.y,luminance)&&Near(p.z,luminance),"Saturation zero is not luminance gray");
    look={};look.temperature=1;auto warm=f.Render(9,9,look,{1,1,1,1});look.temperature=-1;auto cool=f.Render(9,9,look,{1,1,1,1});
    Require(warm[0].x>warm[0].z&&cool[0].x<cool[0].z,"Temperature warm/cool ordering");
    Require(Near(warm[0].x*0.2126f+warm[0].y*0.7152f+warm[0].z*0.0722f,1),"White balance changes gray luminance");
    look={};look.tint=1;auto magenta=f.Render(9,9,look,{1,1,1,1});look.tint=-1;auto green=f.Render(9,9,look,{1,1,1,1});
    Require(magenta[0].x>magenta[0].y&&green[0].x<green[0].y,"Tint direction");
    look={};look.contrast=2;pixels=f.Render(9,9,look,{0.18f,0.09f,0.36f,1});
    Require(Near(pixels[0].x,0.18f)&&Near(pixels[0].y,0.045f)&&Near(pixels[0].z,0.72f),"Contrast must pivot about 18% gray");
    look={};look.vignetteIntensity=0.8f;pixels=f.Render(65,33,look,{1,1,1,1});
    Require(Near(pixels[16*65+32].x,1)&&pixels[0].x<0.25f,"Vignette center or corner attenuation");
    Require(Near(pixels[0].x,pixels[64].x)&&Near(pixels[0].x,pixels[32*65].x),"Vignette symmetry");
    look={};look.bloomEnabled=true;look.bloomIntensity=1;look.bloomSoftKnee=0;
    pixels=f.Render(65,33,look,{0.25f,0.25f,0.25f,1});
    for(const auto& p:pixels)Require(Near(p.x,0.25f),"Below-threshold image blooms");
    pixels=f.Render(65,33,look,{4,4,4,1});
    for(const auto& p:pixels)Require(Near(p.x,7,0.001f),"Normalized bloom pyramid fails constant 4 + (4-1) = 7");
    look.bloomSoftKnee=0.5f;pixels=f.Render(65,33,look,{1,1,1,1});
    Require(Near(pixels[0].x,1.125f,0.001f),"Soft knee continuity at threshold");
    look.bloomSoftKnee=0;pixels=f.Render(65,33,look,{0,0,0,1},RenderMode::Final,true);
    const auto center=16*65+32;
    Require(pixels[center].x>64&&pixels[center+5].x>0.001f,"Bloom failed to spread an HDR impulse into neighboring black pixels");
    auto narrow=pixels;look.bloomRadius=2;auto wide=f.Render(65,33,look,{0,0,0,1},RenderMode::Final,true);
    double difference=0;for(size_t i=0;i<wide.size();++i)difference+=std::abs(narrow[i].x-wide[i].x);
    Require(difference>0.01,"Bloom radius did not change spatial filtering");
    look.bloomIntensity=0;pixels=f.Render(65,33,look,{0,0,0,1},RenderMode::Final,true);
    Require(pixels[center].x==64&&pixels[center+5].x==0,"Zero bloom intensity leaks a stale pyramid");
    look.bloomIntensity=1;look.bloomEnabled=false;pixels=f.Render(65,33,look,{0,0,0,1},RenderMode::Final,true);
    Require(pixels[center+5].x==0,"Disabled bloom leaks previous frame output");
    look.bloomEnabled=true;look.exposure=2;look.temperature=1;look.tint=1;look.saturation=0;look.contrast=2;look.vignetteIntensity=1;
    for(auto mode:{RenderMode::Albedo,RenderMode::Normal,RenderMode::Roughness,RenderMode::Metallic,RenderMode::Depth,RenderMode::Wireframe,
        RenderMode::OriginalImage,RenderMode::EstimatedAlbedo,RenderMode::EstimatedNormal,RenderMode::EstimatedRoughness}){
        pixels=f.Render(7,3,look,{0.25f,0.5f,0.75f,0},mode);
        for(const auto& p:pixels)Require(p.x==0.25f&&p.y==0.5f&&p.z==0.75f&&p.w==0,"Data view modified by post processing");
    }
    look={};look.bloomEnabled=true;look.bloomIntensity=1;look.bloomThreshold=0;look.bloomSoftKnee=0;
    for(auto size:{XMUINT2{1,1},XMUINT2{1,13},XMUINT2{13,1},XMUINT2{17,9},XMUINT2{128,64},XMUINT2{1,1}}){
        pixels=f.Render(size.x,size.y,look,{2,2,2,1});
        for(const auto& p:pixels)Require(Near(p.x,4,0.001f),"Tiny/odd/resized target or descriptor reuse failed");
    }
    f.context.CheckMessages();
}
}
int main(){try{ParameterTests();GpuTests();std::cout<<"PASS: "<<checks<<" post-processing checks (real GPU HDR passes, bloom spread/energy, grading, vignette, bypass, resize, parameter schema)\n";return 0;}
catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
