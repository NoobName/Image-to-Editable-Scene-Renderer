#include "Renderer/ImageRelightingRenderer.h"
#include "Renderer/TextureReadback.h"
#include "Core/Log.h"
#include <cstring>
#include <cmath>
#include <fstream>
#include <iostream>
using namespace isr;
namespace {
void Require(bool p,const char* reason){if(!p)throw std::runtime_error(reason);}
std::shared_ptr<AnalysisMaps> Fixture(){
    auto data=std::make_shared<AnalysisMaps>();constexpr UINT w=17,h=9;
    data->metadata={{"analysisSize",{w,h}}};
    for(size_t i:{1,3,4}){NumericImage im;im.width=w;im.height=h;im.format=i==4?NumericFormat::Label:NumericFormat::Vector;
        im.bytes.resize(w*h*im.Channels()*4);data->maps[i].image=std::move(im);}
    for(UINT p=0;p<w*h;++p){const float x=(float(p%w)-8)/10,y=(float(p/w)-4)/10,z=-std::sqrt(1-x*x-y*y);
        const float n[]{x,y,z,0};std::memcpy(data->maps[1].image->bytes.data()+p*16,n,16);
        const float point[]{x,y,2,0};std::memcpy(data->maps[3].image->bytes.data()+p*16,point,16);
        const uint32_t valid=p%11?1:0;std::memcpy(data->maps[4].image->bytes.data()+p*4,&valid,4);}
    return data;
}
}
int wmain(int argc,wchar_t** argv){try{
    const std::filesystem::path output=argc>1?argv[1]:L"image-shading-tests";std::filesystem::create_directories(output);OpenLog(output/"gpu.log");
    DeviceContext context(argc<3||std::wstring(argv[2])!=L"--hardware");
    DescriptorAllocator resources(context.Device(),D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,32,true);
    ComPtr<ID3D12CommandAllocator> allocator;ComPtr<ID3D12GraphicsCommandList> list;
    Check(context.Device()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator)));
    Check(context.Device()->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&list)));
    auto submit=[&]{Check(list->Close());ID3D12CommandList* lists[]{list.Get()};context.Queue()->ExecuteCommandLists(1,lists);context.Flush();context.CheckMessages();};
    auto reset=[&]{Check(allocator->Reset());Check(list->Reset(allocator.Get(),nullptr));};
    auto cpu=Fixture();AnalysisTextures maps(context.Device(),list.Get(),resources,cpu);ImageRelightingRenderer renderer(context.Device(),maps);
    submit();maps.FinishUpload();LightingSession state;state.available=true;state.source.directIntensity=2;state.source.ambientIntensity=.3f;state.target=state.source;
    package::Json results=package::Json::array();double maxError=0;
    for(unsigned scenario=0;scenario<9;++scenario){
        if(scenario==1)state.target.direction={1,0,0};
        if(scenario==2)state.target.direction={0,0,-1};
        if(scenario==3){state.target.directIntensity=0;state.target.ambientIntensity=.7f;}
        if(scenario==4){state.target.directIntensity=2;state.target.directColor={.2f,.6f,1};state.target.ambientColor={1,.3f,.1f};state.target.direction={0,0,1};}
        if(scenario==5){state.target.directIntensity=0;state.target.ambientIntensity=0;}
        if(scenario==6)state.target.direction={0,0,0};
        if(scenario==7)state.target=state.source;
        if(scenario==8){state.draft=state.source;state.draft.directIntensity=1;Require(state.ApplySource(),"Source calibration rejected");}
        reset();renderer.Update(list.Get(),state);TextureReadback old(context.Device(),list.Get(),*renderer.Old().Image().texture),next(context.Device(),list.Get(),*renderer.New().Image().texture);submit();
        auto a=old.Read(),b=next.Read();Require(old.Before()==D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,"Readback source state incorrect");
        for(size_t p=0;p<17*9;++p)for(unsigned which=0;which<2;++which){const auto& actual=which?b:a;const auto& light=which?state.target:state.source;
            double q=0,dot=0,norm=0;for(unsigned c=0;c<3;++c){double n=cpu->maps[1].image->FloatAt(p,c);norm+=n*n;q+=double(light.direction[c])*light.direction[c];dot-=n*light.direction[c];}
            const bool valid=cpu->maps[4].image->UintAt(p)&&q>1e-12;Require(actual.FloatAt(p,3)==(valid?1.f:0.f),"Shading alpha validity mismatch");
            for(unsigned c=0;c<3;++c){const double expected=valid?std::max(dot/std::sqrt(q*norm),0.)*light.directColor[c]*light.directIntensity+light.ambientColor[c]*light.ambientIntensity:0;
                maxError=std::max(maxError,std::abs(actual.FloatAt(p,c)-expected));}
        }
        if(scenario==0||scenario==7)Require(a.bytes==b.bytes,"Source=target must be byte identical");
        const auto r=renderer.Report();Require(r["oldUpdates"]==(scenario==8?2:1),"Target changes recomputed old shading");
        if(scenario==8)Require(r["newUpdates"]==8,"Source change unexpectedly recomputed target");
        results.push_back({{"scenario",scenario},{"maxError",maxError},{"report",r}});
    }
    reset();renderer.Update(list.Get(),state);submit();Require(renderer.Report()["oldUpdates"]==2&&renderer.Report()["newUpdates"]==8,"Unchanged frame recalculated shading");
    Require(maxError<2e-6,"Analytic GPU tolerance exceeded");Require(context.Warnings()==0,"Debug warning");
    // Target-only point lights: analytic inverse-square falloff, finite cutoff,
    // coincident receiver, color, disabled/zero identity and old-cache isolation.
    std::vector<ImagePointLight> points(1);points[0].position={0,0,1};points[0].color={1,.3f,.1f};points[0].range=3;
    std::vector<uint8_t> baseline,oldBytes;double pointError=0;
    for(unsigned scenario=0;scenario<9;++scenario){
        if(scenario==1)points[0].position[0]=.6f;
        if(scenario==2)points[0].range=.1f;
        if(scenario==3){points[0].range=3;points[0].enabled=false;}
        if(scenario==4){points[0].enabled=true;points[0].intensity=0;}
        if(scenario==5){points[0].intensity=1;points[0].position={0,0,2};}
        if(scenario==6){points.push_back(points[0]);points[1].id=2;points[1].position={-.5f,0,1};points[1].color={.1f,.5f,1};}
        if(scenario==7)state.targetGlobalGain=0;
        if(scenario==8){points.clear();state.targetGlobalGain=1;}
        reset();renderer.Update(list.Get(),state,scenario?points:std::vector<ImagePointLight>{});
        TextureReadback old(context.Device(),list.Get(),*renderer.Old().Image().texture),next(context.Device(),list.Get(),*renderer.New().Image().texture);submit();auto a=old.Read(),b=next.Read();
        if(scenario==0){baseline=b.bytes;oldBytes=a.bytes;}
        Require(a.bytes==oldBytes&&renderer.Report()["oldUpdates"]==2,"Point edit modified old shading/cache");
        if(scenario==2||scenario==3||scenario==4||scenario==8)Require(b.bytes==baseline,"Point off/out-of-range failed exact identity");
        const auto light=state.EffectiveTarget();
        for(size_t pixel=0;pixel<17*9;++pixel){const bool valid=cpu->maps[4].image->UintAt(pixel)!=0;double n[3],q=0,norm=0,dot=0;
            for(unsigned c=0;c<3;++c){n[c]=cpu->maps[1].image->FloatAt(pixel,c);norm+=n[c]*n[c];q+=double(light.direction[c])*light.direction[c];dot-=n[c]*light.direction[c];}
            for(unsigned c=0;c<3;++c){double expected=valid?std::max(dot/std::sqrt(q*norm),0.)*light.directColor[c]*light.directIntensity+light.ambientColor[c]*light.ambientIntensity:0;
                if(valid&&scenario)for(const auto& point:points)if(point.enabled&&point.intensity>0){double d2=0,nd=0;
                    for(unsigned axis=0;axis<3;++axis){const double d=point.position[axis]-cpu->maps[3].image->FloatAt(pixel,axis);d2+=d*d;nd+=n[axis]*d;}
                    const double window=std::clamp(1-std::pow(std::sqrt(std::max(d2,.0001))/point.range,4),0.,1.);
                    expected+=std::max(nd/std::sqrt(norm*std::max(d2,1e-12)),0.)*window*window/std::max(d2,.0001)*point.color[c]*point.intensity*state.targetGlobalGain;
                }
                Require(std::isfinite(b.FloatAt(pixel,c)),"Point output NaN/Inf");pointError=std::max(pointError,std::abs(b.FloatAt(pixel,c)-expected));
            }
        }
        results.push_back({{"pointScenario",scenario},{"maxError",pointError},{"oldCacheUnchanged",true},{"report",renderer.Report()}});
    }
    Require(pointError<5e-5,"Point CPU/GPU analytic mismatch");Require(context.Warnings()==0,"Point debug warning");
    std::ofstream out(output/"results.json");out<<results.dump(2)<<'\n';std::cout<<"cases=18 maxError="<<maxError<<" pointError="<<pointError<<" nonfinite=0\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
