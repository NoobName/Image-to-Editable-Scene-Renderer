#pragma once
#include "Renderer/EnvironmentBaker.h"
#include <map>
namespace isr {
class EnvironmentManager {
public:
    EnvironmentManager(DeviceContext&,DescriptorAllocator&,UINT firstView);
    void Load(const std::filesystem::path&);
    bool TryLoad(const std::filesystem::path&);
    const std::filesystem::path& Path() const{return path_;}
    const std::vector<std::filesystem::path>& Available() const{return available_;}
    const std::string& Error() const{return error_;}
    D3D12_GPU_DESCRIPTOR_HANDLE SkyView() const{return heap_.Gpu(firstView_+3);}
    static std::filesystem::path DefaultPath();
private:
    DeviceContext& context_;
    DescriptorAllocator& heap_;
    UINT firstView_;
    EnvironmentBaker baker_;
    EnvironmentMaps current_;
    struct Cached {std::filesystem::file_time_type modified;EnvironmentMaps maps;};
    std::map<std::filesystem::path,Cached> cache_;
    std::vector<std::filesystem::path> available_;
    std::filesystem::path path_;
    std::string error_;
};
}
