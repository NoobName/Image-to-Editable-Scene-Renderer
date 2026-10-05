#pragma once
#include "Renderer/DescriptorAllocator.h"
#include "Renderer/UploadBuffer.h"
#include "Scene/Scene.h"
#include <map>
namespace isr {
struct MaterialBinding { D3D12_GPU_DESCRIPTOR_HANDLE textures{},samplers{}; };
class TextureManager {
public:
    TextureManager(ID3D12Device*,ID3D12GraphicsCommandList*,DescriptorAllocator&,DescriptorAllocator&,const Scene&);
    const MaterialBinding& Binding(size_t material) const { return bindings_.at(material); }
    void FinishUpload();
    size_t ResourceCount() const { return resources_.size(); }
private:
    struct GpuImage { ComPtr<ID3D12Resource> resource; std::unique_ptr<UploadBuffer> upload; UINT levels{}; bool srgb{}; };
    GpuImage& Upload(ID3D12Device*,ID3D12GraphicsCommandList*,std::shared_ptr<const ImageData>,bool);
    std::map<std::pair<const ImageData*,bool>,GpuImage> resources_;
    std::vector<std::shared_ptr<const ImageData>> retainedImages_;
    std::vector<MaterialBinding> bindings_;
};
}
