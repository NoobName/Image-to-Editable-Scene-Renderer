#pragma once
#include "Renderer/NumericTexture.h"
#include "Renderer/FullscreenPass.h"
#include "Renderer/TextureReadback.h"
#include "ScenePackage/RelightingSession.h"
namespace isr {
class LightingPreview {
public:
    LightingPreview(ID3D12Device*,ID3D12GraphicsCommandList*,DescriptorAllocator&,std::shared_ptr<const LightingData>);
    void FinishUpload();
    void Draw(ID3D12GraphicsCommandList*,D3D12_CPU_DESCRIPTOR_HANDLE,uint32_t,uint32_t,uint32_t,uint32_t,size_t,bool)const;
    NumericTexture* Map(size_t i)const{return maps_.at(i).get();}
    const LightingData* Data()const{return data_.get();}
private:
    std::shared_ptr<const LightingData> data_;
    std::array<std::unique_ptr<NumericTexture>,4> maps_;
    DescriptorAllocation nullFloat_,nullUint_;
    FullscreenPass pass_;
};
class LightingCapture {
public:
    LightingCapture(ID3D12Device*,ID3D12GraphicsCommandList*,LightingPreview&);
    void Save(const std::filesystem::path&)const;
private:
    const LightingData* cpu_;
    std::array<std::unique_ptr<TextureReadback>,4> reads_;
};
}
