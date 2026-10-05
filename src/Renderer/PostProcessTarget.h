#pragma once
#include "Renderer/DescriptorAllocator.h"
#include "Renderer/Texture.h"
#include <memory>
namespace isr {
inline constexpr DXGI_FORMAT PostProcessFormat=DXGI_FORMAT_R32G32B32A32_FLOAT;
struct PostProcessImage { Texture* texture;D3D12_GPU_DESCRIPTOR_HANDLE srv; };
class PostProcessTarget {
public:
    PostProcessTarget(DescriptorAllocator& rtvHeap,DescriptorAllocator& resourceHeap)
        :rtv_(rtvHeap.Allocate()),srv_(resourceHeap.Allocate()){}
    // Caller waits for all submitted uses before resizing or overwriting views.
    void Resize(ID3D12Device* device,UINT width,UINT height){
        if(texture_&&width==width_&&height==height_)return;
        if(!width||!height)throw std::invalid_argument("Zero-sized post-process target");
        texture_=std::make_unique<Texture>(device,width,height,PostProcessFormat,D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        device->CreateRenderTargetView(texture_->Resource(),nullptr,rtv_.cpu);
        D3D12_SHADER_RESOURCE_VIEW_DESC desc{};desc.Format=PostProcessFormat;desc.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;
        desc.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;desc.Texture2D.MipLevels=1;
        device->CreateShaderResourceView(texture_->Resource(),&desc,srv_.cpu);width_=width;height_=height;
    }
    void Begin(ID3D12GraphicsCommandList* list){texture_->Transition(list,D3D12_RESOURCE_STATE_RENDER_TARGET);}
    void End(ID3D12GraphicsCommandList* list){texture_->Transition(list,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);}
    PostProcessImage Image() const {return {texture_.get(),srv_.gpu};}
    D3D12_CPU_DESCRIPTOR_HANDLE Rtv() const {return rtv_.cpu;}
    UINT Width() const {return width_;} UINT Height() const {return height_;}
private:
    DescriptorAllocation rtv_,srv_;
    std::unique_ptr<Texture> texture_;
    UINT width_=0,height_=0;
};
}
