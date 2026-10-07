#include "Renderer/ImageRelightingComposite.h"
#include "Renderer/TextureReadback.h"
#include "Renderer/FrameCapture.h"
#include "Core/Log.h"
#include <cstring>
#include <cmath>
#include <fstream>
#include <iostream>
using namespace isr;
namespace {
void Require(bool b,const char* why){if(!b)throw std::runtime_error(why);}
double Decode(double c){return c<=.04045?c/12.92:std::pow((c+.055)/1.055,2.4);}
uint8_t Encode(double c){return uint8_t(std::lround(std::clamp(c<=.0031308?12.92*c:1.055*std::pow(c,1/2.4)-.055,0.,1.)*255));}
}
int wmain(int argc,wchar_t** argv){try{
    const std::filesystem::path directory=argc>1?argv[1]:L"image-ratio-tests";std::filesystem::create_directories(directory);OpenLog(directory/"gpu.log");
    DeviceContext context(argc<3||std::wstring(argv[2])!=L"--hardware");DescriptorAllocator heap(context.Device(),D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,32,true);
    DescriptorAllocator rtv(context.Device(),D3D12_DESCRIPTOR_HEAP_TYPE_RTV,1);auto output=rtv.Allocate();
    ComPtr<ID3D12CommandAllocator> allocator;ComPtr<ID3D12GraphicsCommandList> list;
    Check(context.Device()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator)));
    Check(context.Device()->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&list)));
    auto submit=[&]{Check(list->Close());ID3D12CommandList* lists[]{list.Get()};context.Queue()->ExecuteCommandLists(1,lists);context.Flush();context.CheckMessages();};
    auto reset=[&]{Check(allocator->Reset());Check(list->Reset(allocator.Get(),nullptr));};
    auto data=std::make_shared<AnalysisMaps>();data->metadata={{"analysisSize",{9,5}}};
    for(size_t i:{1,4}){NumericImage im;im.width=9;im.height=5;im.format=i==1?NumericFormat::Vector:NumericFormat::Label;im.bytes.resize(45*im.Channels()*4);data->maps[i].image=std::move(im);}
    for(UINT p=0;p<45;++p){const float nx=(float(p%9)-4)*.2f;const float n[]{nx,0,-std::sqrt(1-nx*nx),0};
        std::memcpy(data->maps[1].image->bytes.data()+p*16,n,16);uint32_t v=p%4?1:0;std::memcpy(data->maps[4].image->bytes.data()+p*4,&v,4);}
    ImageData original;original.width=257;original.height=193;original.rgba.resize(257*193*4);
    for(UINT y=0;y<193;++y)for(UINT x=0;x<257;++x){const size_t p=(y*257+x)*4;original.rgba[p]=uint8_t(x%256);original.rgba[p+1]=uint8_t((x^y)%256);original.rgba[p+2]=uint8_t((y*17+x*3)%256);original.rgba[p+3]=255;}
    SourceImagePass source(context.Device(),list.Get(),heap,original);AnalysisTextures maps(context.Device(),list.Get(),heap,data);
    ImageRelightingRenderer shading(context.Device(),maps);ImageRelightingComposite composite(context.Device(),list.Get(),source,shading,&maps);submit();maps.FinishUpload();source.FinishUpload();composite.FinishUpload();
    LightingSession lighting;lighting.available=true;lighting.source.directIntensity=1;lighting.source.ambientIntensity=.2f;lighting.target=lighting.source;
    RelightingParameters parameters;parameters.stability=false;double maxError=0;unsigned maxLSB=0;package::Json report=package::Json::array();
    for(unsigned scenario=0;scenario<14;++scenario){
        parameters.strength=1;lighting.target=lighting.source;
        if(scenario%2==1)lighting.target.directIntensity=1.7f;
        if(scenario==3)parameters.strength=0;
        if(scenario==5)lighting.target.directColor={.2f,1,.4f};
        if(scenario==7){lighting.source.directIntensity=lighting.source.ambientIntensity=0;lighting.target=lighting.source;}
        if(scenario==9){lighting.source.directIntensity=1e-8f;lighting.target=lighting.source;lighting.target.directIntensity=2;}
        reset();shading.Update(list.Get(),lighting);composite.Update(list.Get(),parameters);
        TextureReadback ratios(context.Device(),list.Get(),*composite.Ratio().Image().texture),result(context.Device(),list.Get(),*composite.Final().Image().texture);
        Texture screen(context.Device(),257,193,DXGI_FORMAT_R8G8B8A8_UNORM,D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,D3D12_RESOURCE_STATE_RENDER_TARGET);
        context.Device()->CreateRenderTargetView(screen.Resource(),nullptr,output.cpu);composite.Draw(list.Get(),output.cpu,257,193,1);FrameCapture capture(context.Device(),list.Get(),screen.Resource());submit();
        const auto ratio=ratios.Read(),image=result.Read();auto expected=original;
        for(UINT y=0;y<193;++y)for(UINT x=0;x<257;++x){const size_t p=y*257+x;const UINT ax=UINT((x+.5)*9/257),ay=UINT((y+.5)*5/193);
            const bool valid=data->maps[4].image->UintAt(ay*9+ax)!=0;double a[3],b[3],ya=0,yb=0;constexpr double lum[]{.2126,.7152,.0722};
            const double cosine=-data->maps[1].image->FloatAt(ay*9+ax,2);
            for(UINT c=0;c<3;++c){a[c]=cosine*lighting.source.directIntensity*lighting.source.directColor[c]+lighting.source.ambientIntensity*lighting.source.ambientColor[c];
                b[c]=cosine*lighting.target.directIntensity*lighting.target.directColor[c]+lighting.target.ambientIntensity*lighting.target.ambientColor[c];ya+=a[c]*lum[c];yb+=b[c]*lum[c];}
            const double yr=(yb+parameters.epsilon)/(ya+parameters.epsilon);
            for(UINT c=0;c<3;++c){double r=1;if(valid&&parameters.strength!=0){const double cr=std::clamp((b[c]+parameters.epsilon)/(a[c]+parameters.epsilon)/yr,.5,2.);
                    r=std::pow(std::clamp(yr*cr,double(parameters.minRatio),double(parameters.maxRatio)),parameters.strength);}
                maxError=std::max(maxError,std::abs(ratio.FloatAt(p,c)-r));const double linear=Decode(double(original.rgba[p*4+c])/255)*r;
                maxError=std::max(maxError,std::abs(image.FloatAt(p,c)-linear));expected.rgba[p*4+c]=Encode(linear);
                if(!valid||scenario%2==0||scenario==3||scenario==7)Require(std::abs(ratio.FloatAt(p,c)-1)<1e-7,"Identity ratio not one");}
        }
        maxLSB=std::max(maxLSB,capture.CompareRgb(expected,1));report.push_back({{"scenario",scenario},{"maxError",maxError},{"maxLSB",maxLSB}});
    }
    std::cout<<"Numeric maxError="<<maxError<<" maxLSB="<<maxLSB<<'\n';
    Require(maxError<5e-5,"Numeric ratio reference exceeded tolerance");Require(context.Warnings()==0,"Debug Layer warning");
    std::ofstream out(directory/"results.json");out<<report.dump(2)<<'\n';std::cout<<"14 cases maxError="<<maxError<<" maxLSB="<<maxLSB<<" finite=1 reset=no-drift\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
