#pragma once
#include "Renderer/ImageFullscreenPass.h"
#include "Renderer/NumericTexture.h"
#include "Renderer/TextureReadback.h"
#include "ScenePackage/RelightingSession.h"
namespace isr {
// Diagnostics own a separate heap; no shadow support map is bound to the RGB composite.
class ShadowPreview {
public:
    ShadowPreview(ID3D12Device*,ID3D12GraphicsCommandList*,Texture&,std::shared_ptr<const ShadowData>);
    void FinishUpload();
    void Draw(ID3D12GraphicsCommandList*,D3D12_CPU_DESCRIPTOR_HANDLE,uint32_t,uint32_t,uint32_t,uint32_t,size_t,bool)const;
    NumericTexture* Map(size_t i)const{return maps_.at(i).get();}
    const ShadowData* Data()const{return data_.get();}
private:
    std::shared_ptr<const ShadowData> data_;
    DescriptorAllocator heap_;
    std::array<std::unique_ptr<NumericTexture>,8> maps_;
    DescriptorAllocation nullFloat_,nullUint_,source_;
    ImageFullscreenPass pass_;
};
class ShadowCapture {
public:
    ShadowCapture(ID3D12Device*,ID3D12GraphicsCommandList*,ShadowPreview&);
    void Save(const std::filesystem::path&)const;
private:
    const ShadowData* cpu_;
    std::array<std::unique_ptr<TextureReadback>,8> reads_;
};
}
