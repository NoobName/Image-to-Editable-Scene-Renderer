#pragma once
#include "Renderer/FullscreenPass.h"
#include "Renderer/Texture.h"
#include "Renderer/UploadBuffer.h"
#include "Renderer/DescriptorAllocator.h"
#include "ScenePackage/RelightingSession.h"
namespace isr {
class SourceImagePass {
public:
    SourceImagePass(ID3D12Device*,ID3D12GraphicsCommandList*,DescriptorAllocator&,const ImageData&);
    void FinishUpload(){upload_.reset();}
    void Draw(ID3D12GraphicsCommandList*,D3D12_CPU_DESCRIPTOR_HANDLE,uint32_t width,uint32_t height,ImageDebugView)const;
private:
    Texture texture_;
    std::unique_ptr<UploadBuffer> upload_;
    DescriptorAllocation view_;
    FullscreenPass pass_;
    uint32_t width_,height_;
};
}
