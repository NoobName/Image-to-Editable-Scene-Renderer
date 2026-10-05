#pragma once
#include "Assets/NumericDds.h"
#include "Renderer/Texture.h"
#include "Renderer/UploadBuffer.h"
#include "Renderer/DescriptorAllocator.h"
namespace isr {
class NumericTexture {
public:
    NumericTexture(ID3D12Device*,ID3D12GraphicsCommandList*,DescriptorAllocator&,const NumericImage&);
    void FinishUpload(){upload_.reset();}
    Texture& Image(){return texture_;}
    D3D12_GPU_DESCRIPTOR_HANDLE View()const{return view_.gpu;}
private:
    Texture texture_;
    DescriptorAllocation view_;
    std::unique_ptr<UploadBuffer> upload_;
};
}
