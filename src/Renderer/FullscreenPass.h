#pragma once
#include "Renderer/RootSignature.h"
#include "Renderer/PipelineState.h"
#include <filesystem>
#include <memory>
namespace isr {
// Shared fullscreen primitive: t0/t1, linear-clamp s0, compact b0 constants.
// The caller binds the shader-visible heap containing both supplied SRVs.
class FullscreenPass {
public:
    FullscreenPass(ID3D12Device*,const std::filesystem::path&,const wchar_t* pixelEntry,DXGI_FORMAT,unsigned constantCount);
    void Draw(ID3D12GraphicsCommandList*,D3D12_CPU_DESCRIPTOR_HANDLE output,UINT width,UINT height,
        D3D12_GPU_DESCRIPTOR_HANDLE input,D3D12_GPU_DESCRIPTOR_HANDLE secondary,const void* constants) const;
private:
    unsigned constantCount_;
    std::unique_ptr<RootSignature> root_;
    std::unique_ptr<PipelineState> pipeline_;
};
}
