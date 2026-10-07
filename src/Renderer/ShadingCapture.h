#pragma once
#include "Renderer/ImageRelightingRenderer.h"
#include "Renderer/TextureReadback.h"
#include "Renderer/ImageRelightingComposite.h"
namespace isr {
class ShadingCapture {
public:
    ShadingCapture(ID3D12Device*,ID3D12GraphicsCommandList*,ImageRelightingRenderer&,ImageRelightingComposite* = nullptr,package::Json timer={});
    void Save(const std::filesystem::path&)const;
private:
    package::Json report_;
    std::array<std::unique_ptr<TextureReadback>,20> reads_;
};
}
