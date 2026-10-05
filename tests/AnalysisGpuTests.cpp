#include "Renderer/AnalysisTextures.h"
#include "Renderer/TextureReadback.h"
#include "Renderer/AnalysisCapture.h"
#include "ScenePackage/ScenePackageLoader.h"
#include "Core/Log.h"
#include <cstring>
#include <cmath>
#include <fstream>
#include <iostream>
using namespace isr;
namespace {
void Require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
std::shared_ptr<AnalysisMaps> Fixture(const std::string& kind){
    constexpr unsigned w=65,h=33;auto maps=std::make_shared<AnalysisMaps>();
    maps->metadata={{"analysisSize",{w,h}},{"sampling",{{"relativeDepthThreshold",.15}}}};
    for(size_t i=0;i<13;++i)maps->maps[i].metadata={{"status","unavailable"},{"reason","not part of analytic fixture"}};
    for(const size_t i:{0,1,2,3,4,5}){auto& map=maps->maps[i];NumericImage image;
        image.width=w;image.height=h;image.format=i>=4?NumericFormat::Label:i==0?NumericFormat::Float:NumericFormat::Vector;
        image.bytes.resize(size_t(w)*h*image.Channels()*4);map.image=std::move(image);map.metadata={{"range",{-4,7}}};}
    auto put=[&](size_t map,size_t pixel,unsigned c,float value){auto& im=*maps->maps[map].image;std::memcpy(im.bytes.data()+(pixel*im.Channels()+c)*4,&value,4);};
    auto putUint=[&](size_t map,size_t pixel,uint32_t value){std::memcpy(maps->maps[map].image->bytes.data()+pixel*4,&value,4);};
    for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x){const size_t p=size_t(y)*w+x;
        const bool valid=kind!="step"||!(x>=9&&x<13&&y>=8&&y<15);
        const float rx=(float(x)+.5f)/w-.5f,ry=-(float(y)+.5f-float(h)/2)/w;
        const float z=!valid?0:kind=="step"?(x<w/2?3.f:7.f):kind=="tilted"?3/(1-.2f*rx-.3f*ry):3;
        put(0,p,0,z);put(3,p,0,rx*z);put(3,p,1,ry*z);put(3,p,2,z);
        // Adjacent unit normals deliberately differ: GPU must normalize their interpolation.
        const float nx=kind=="tilted"&&x>w/2?.5f:0,ny=kind=="tilted"?.25f:0,nz=-std::sqrt(1-nx*nx-ny*ny);
        for(size_t i:{1,2}){put(i,p,0,valid?nx:0);put(i,p,1,valid?ny:0);put(i,p,2,valid?nz:0);}
        putUint(4,p,valid?1u:0u);putUint(5,p,x<w/2?16777217u:4294967295u);
    }
    return maps;
}
}
int wmain(int argc,wchar_t** argv){try{
    std::filesystem::path output=argc>2?argv[2]:L"analysis-gpu-tests";
    std::filesystem::create_directories(output);OpenLog(output/"gpu.log");
    const bool warp=argc<4||std::wstring(argv[3])!=L"--hardware";
    DeviceContext context(warp);package::Json report=package::Json::array();
    for(const auto& kind:argc>1?std::vector<std::string>{std::filesystem::path(argv[1]).filename().string()}:std::vector<std::string>{"plane","tilted","step"}){
        std::shared_ptr<const AnalysisMaps> cpu=argc>1?ScenePackageLoader{}.Load(argv[1]).observation->analysisMaps:Fixture(kind);
        Require(cpu!=nullptr,"Package has no runtime maps");
        const unsigned w=cpu->metadata["analysisSize"][0],h=cpu->metadata["analysisSize"][1];
        DescriptorAllocator heap(context.Device(),D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,32,true);
        DescriptorAllocator rtv(context.Device(),D3D12_DESCRIPTOR_HEAP_TYPE_RTV,1);auto view=rtv.Allocate();
        ComPtr<ID3D12CommandAllocator> allocator;ComPtr<ID3D12GraphicsCommandList> list;
        Check(context.Device()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator)));
        Check(context.Device()->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&list)));
        auto submit=[&]{Check(list->Close());ID3D12CommandList* lists[]{list.Get()};context.Queue()->ExecuteCommandLists(1,lists);context.Flush();context.CheckMessages();};
        auto reset=[&]{Check(allocator->Reset());Check(list->Reset(allocator.Get(),nullptr));};
        AnalysisTextures maps(context.Device(),list.Get(),heap,cpu);AnalysisCapture original(context.Device(),list.Get(),maps);
        submit();maps.FinishUpload();original.Save(output/(kind+".bmp"));
        double largestNormalError=0,largestNativeError=0;size_t invalidSamples=0,edgeSamples=0;
        for(unsigned scale:{1,2}){
            std::array<NumericImage,3> sampled;
            for(size_t slot=0;slot<3;++slot){const size_t selected=slot==2?3:slot;
                reset();Texture target(context.Device(),w*scale,h*scale,DXGI_FORMAT_R32G32B32A32_FLOAT,D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,D3D12_RESOURCE_STATE_RENDER_TARGET);
                context.Device()->CreateRenderTargetView(target.Resource(),nullptr,view.cpu);ID3D12DescriptorHeap* heaps[]{heap.Heap()};list->SetDescriptorHeaps(1,heaps);
                maps.Draw(list.Get(),view.cpu,w*scale,h*scale,w,h,selected,true);
                TextureReadback readback(context.Device(),list.Get(),target);
                Require(readback.Before()==D3D12_RESOURCE_STATE_RENDER_TARGET&&target.State()==D3D12_RESOURCE_STATE_RENDER_TARGET,"RT readback state not restored");submit();sampled[slot]=readback.Read();
                std::ofstream bytes(output/(kind+"-sample-"+std::to_string(scale)+"-"+std::string(AnalysisKeys[selected])+".bin"),std::ios::binary);
                bytes.write(reinterpret_cast<const char*>(sampled[slot].bytes.data()),sampled[slot].bytes.size());
            }
            for(unsigned y=0;y<h*scale;++y)for(unsigned x=0;x<w*scale;++x){const size_t p=size_t(y)*w*scale+x,center=size_t(y/scale)*w+x/scale;
                const bool valid=cpu->maps[4].image->UintAt(center)!=0;
                for(size_t slot=0;slot<3;++slot){const auto& actual=sampled[slot];Require(actual.FloatAt(p,3)==(valid?1.f:0.f),"Sampling validity mismatch");
                    if(!valid){++invalidSamples;for(unsigned c=0;c<4;++c)Require(actual.FloatAt(p,c)==0,"Invalid footprint invented a value");}
                    if(scale==1){const auto& expected=*cpu->maps[slot==2?3:slot].image;
                        for(unsigned c=0;c<expected.Channels()&&c<3;++c)largestNativeError=std::max(largestNativeError,double(std::abs(actual.FloatAt(p,c)-expected.FloatAt(center,c))));}
                }
                if(!valid)continue;
                double length=0;for(unsigned c=0;c<3;++c){const double n=sampled[1].FloatAt(p,c);length+=n*n;}
                largestNormalError=std::max(largestNormalError,std::abs(std::sqrt(length)-1));
                Require(std::abs(sampled[0].FloatAt(p)-sampled[2].FloatAt(p,2))<2e-5,"Depth and position Z disagree");
                if(kind=="step"){const float z=sampled[0].FloatAt(p);Require(std::abs(z-3)<1e-5||std::abs(z-7)<1e-5,"Depth edge produced a floating position");++edgeSamples;}
                if(y==0)Require(sampled[2].FloatAt(p,1)>0,"LH camera top must have positive Y");
                if(y==h*scale-1)Require(sampled[2].FloatAt(p,1)<0,"LH camera bottom must have negative Y");
            }
        }
        Require(largestNativeError<2e-5&&largestNormalError<1e-5,"Numeric shader tolerance exceeded");
        report.push_back({{"case",kind},{"size",{w,h}},{"nativeMaxError",largestNativeError},{"normalMaxLengthError",largestNormalError},
            {"invalidSamples",invalidSamples},{"edgeSamples",edgeSamples},{"nonfinite",0},{"uploadByteExact",true}});
        std::cout<<report.back().dump()<<'\n';
    }
    std::ofstream result(output/"results.json");result<<report.dump(2)<<'\n';context.CheckMessages();
    Require(context.Warnings()==0,"Debug Layer warning");return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
