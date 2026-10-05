#pragma once
#include "Renderer/FullscreenPass.h"
#include "Renderer/RenderSettings.h"
#include <memory>
namespace isr {
class ToneMapPass {
public:
    explicit ToneMapPass(ID3D12Device*);
    void Draw(ID3D12GraphicsCommandList*,D3D12_CPU_DESCRIPTOR_HANDLE,UINT width,UINT height,
        D3D12_GPU_DESCRIPTOR_HANDLE,RenderMode,const LookParameters&);
private:
    FullscreenPass pass_;
};
}
