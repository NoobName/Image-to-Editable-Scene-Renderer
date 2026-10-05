#pragma once
#include "Renderer/RootSignature.h"
#include "Renderer/PipelineState.h"
#include "Renderer/FrameContext.h"
#include "Renderer/RenderSettings.h"
#include "Scene/Camera.h"
namespace isr {
class SkyPass {
public:
    explicit SkyPass(ID3D12Device*);
    void Draw(ID3D12GraphicsCommandList*,FrameContext&,const Camera&,D3D12_GPU_DESCRIPTOR_HANDLE,const RenderSettings&);
private:
    std::unique_ptr<RootSignature> root_;
    std::unique_ptr<PipelineState> pipeline_;
};
}
