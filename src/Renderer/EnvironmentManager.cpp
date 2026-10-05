#include "Renderer/EnvironmentManager.h"
#include "Renderer/ShaderCompiler.h"
#include "Assets/AssetIO.h"
#include "Core/Log.h"
#include <algorithm>
#include <chrono>
namespace isr {
std::filesystem::path EnvironmentManager::DefaultPath(){return ExecutableDirectory()/"assets/environments/SoftStudio.hdr";}
EnvironmentManager::EnvironmentManager(DeviceContext& context,DescriptorAllocator& heap,UINT firstView)
    :context_(context),heap_(heap),firstView_(firstView),baker_(context){
    auto directory=DefaultPath().parent_path();if(std::filesystem::exists(directory))for(const auto& entry:std::filesystem::directory_iterator(directory))
        if(entry.is_regular_file()&&(entry.path().extension()==L".hdr"||entry.path().extension()==L".HDR"))available_.push_back(entry.path());
    std::sort(available_.begin(),available_.end());
}
void EnvironmentManager::Load(const std::filesystem::path& requested){
    auto path=std::filesystem::canonical(requested);auto modified=std::filesystem::last_write_time(path);
    auto cached=cache_.find(path);EnvironmentMaps maps;const auto start=std::chrono::steady_clock::now();
    // No descriptor overwrite or cache eviction while a submitted frame can reference it.
    context_.Flush();
    if(cached!=cache_.end()&&cached->second.modified==modified){maps=cached->second.maps;Log("Environment GPU cache hit: "+PathUtf8(path));}
    else {
        auto image=LoadHdr(path);Log("Loading HDR: "+PathUtf8(path)+" "+std::to_string(image.width)+"x"+std::to_string(image.height));
        maps=baker_.Bake(std::move(image));
        if(cache_.size()>=3&&!cache_.contains(path))cache_.erase(cache_.begin());
        cache_[path]={modified,maps};
        Log("IBL baked: irradiance 32^2x6; GGX prefilter 128^2x6 / 8 mips; shared BRDF LUT 128^2; ms="+
            std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count()));
    }
    // Publish only a complete environment. Failed decode/bake retains the previous resource set.
    current_=std::move(maps);path_=path;error_.clear();
    WriteViews(heap_,firstView_);
    if(std::find(available_.begin(),available_.end(),path)==available_.end())available_.push_back(path);
    Log("Environment active: "+PathUtf8(path));
}
bool EnvironmentManager::TryLoad(const std::filesystem::path& path){try{Load(path);return true;}catch(const std::exception& e){error_=e.what();Log("Environment load failed; retaining previous environment: "+error_);return false;}}
void EnvironmentManager::WriteViews(DescriptorAllocator& heap,UINT first)const{
    auto* device=context_.Device();EnvironmentBaker::CubeSrv(device,current_.irradiance.Get(),1,heap.Cpu(first));
    EnvironmentBaker::CubeSrv(device,current_.prefilter.Get(),EnvironmentBaker::PrefilterMips,heap.Cpu(first+1));
    EnvironmentBaker::ImageSrv(device,baker_.BrdfLut(),1,heap.Cpu(first+2));
    EnvironmentBaker::CubeSrv(device,current_.sky.Get(),EnvironmentBaker::SkyMips,heap.Cpu(first+3));
}
}
