#pragma once
#include "Renderer/GpuBuffer.h"
#include "Renderer/FrameContext.h"
#include "Renderer/RootSignature.h"
#include "Renderer/PipelineState.h"
#include "Scene/Scene.h"
#include "Renderer/TextureManager.h"
#include "Renderer/RenderSettings.h"
#include "Renderer/ShadowPass.h"
namespace isr {
class ScenePass {
public:
    ScenePass(ID3D12Device*, ID3D12GraphicsCommandList*, DescriptorAllocator&, DescriptorAllocator&, const Scene&);
    void FinishUpload();
    void Draw(ID3D12GraphicsCommandList*, FrameContext&, const Scene&, const RenderSettings&,const ShadowFrame&,D3D12_GPU_DESCRIPTOR_HANDLE lighting,float environmentMaxMip,bool reverseOrder);
    const GpuScene& Assets() const{return *assets_;}
private:
    std::unique_ptr<GpuScene> assets_;
    std::unique_ptr<RootSignature> root_;
    std::array<std::unique_ptr<PipelineState>,16> pipelines_;
};
}
