#pragma once
#include "Renderer/NumericTexture.h"
#include "Renderer/FullscreenPass.h"
#include "Renderer/TextureReadback.h"
#include "ScenePackage/RelightingSession.h"
namespace isr {
class IntrinsicPreview {
public:
    IntrinsicPreview(ID3D12Device*,ID3D12GraphicsCommandList*,DescriptorAllocator&,std::shared_ptr<const IntrinsicData>);
    void FinishUpload();
    void Draw(ID3D12GraphicsCommandList*,D3D12_CPU_DESCRIPTOR_HANDLE,uint32_t,uint32_t,uint32_t,uint32_t,size_t)const;
    NumericTexture* Map(size_t i)const{return maps_.at(i).get();}
    const IntrinsicData* Data()const{return data_.get();}
private:
    std::shared_ptr<const IntrinsicData> data_;
    std::array<std::unique_ptr<NumericTexture>,6> maps_;
    DescriptorAllocation nullFloat_,nullUint_;
    FullscreenPass pass_;
};
class IntrinsicCapture {
public:
    IntrinsicCapture(ID3D12Device*,ID3D12GraphicsCommandList*,IntrinsicPreview&);
    void Save(const std::filesystem::path&)const;
private:
    const IntrinsicData* cpu_;
    std::array<std::unique_ptr<TextureReadback>,6> reads_;
};
}
