#pragma once
#include "Renderer/AnalysisTextures.h"
#include "Renderer/TextureReadback.h"
namespace isr {
class AnalysisCapture {
public:
    AnalysisCapture(ID3D12Device*,ID3D12GraphicsCommandList*,AnalysisTextures&);
    void Save(const std::filesystem::path& capture)const;
private:
    const AnalysisMaps* cpu_;
    std::array<std::unique_ptr<TextureReadback>,13> reads_;
};
}
