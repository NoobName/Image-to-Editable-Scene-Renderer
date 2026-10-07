#pragma once
#include "Renderer/RootSignature.h"
#include "Renderer/PipelineState.h"
#include <filesystem>
#include <memory>
namespace isr {
// Bounded image-domain binding layout. Existing two-input post processing stays unchanged.
class ImageFullscreenPass {
public:
    ImageFullscreenPass(ID3D12Device*,const std::filesystem::path&,const wchar_t*,DXGI_FORMAT,UINT inputs,UINT constants);
    void Draw(ID3D12GraphicsCommandList*,D3D12_CPU_DESCRIPTOR_HANDLE,UINT,UINT,
        std::span<const D3D12_GPU_DESCRIPTOR_HANDLE>,const void*)const;
private:
    UINT inputs_,constants_;
    std::unique_ptr<RootSignature> root_;
    std::unique_ptr<PipelineState> pipeline_;
};
}
