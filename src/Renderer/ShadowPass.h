#pragma once
#include "Renderer/GpuScene.h"
#include "Renderer/Texture.h"
#include "Renderer/FrameContext.h"
#include "Renderer/RootSignature.h"
#include "Renderer/PipelineState.h"
#include "Renderer/RenderSettings.h"
#include "Scene/Bounds.h"
namespace isr {
struct ShadowFrame { DirectX::XMFLOAT4X4 viewProjection{};int lightIndex=-1; };
class ShadowPass {
public:
    static constexpr unsigned Resolution=2048;
    ShadowPass(ID3D12Device*,DescriptorAllocator& dsv,D3D12_CPU_DESCRIPTOR_HANDLE srv,const Scene&);
    ShadowFrame Draw(ID3D12GraphicsCommandList*,FrameContext&,const Scene&,const GpuScene&,const RenderSettings&,const DirectX::XMFLOAT3* fixedDirection=nullptr);
    Texture& Depth(){return *depth_;}
private:
    std::unique_ptr<Texture> depth_;
    DescriptorAllocation dsv_;
    std::unique_ptr<RootSignature> root_;
    std::array<std::unique_ptr<PipelineState>,4> pipelines_;
    std::vector<Bounds> meshBounds_;
};
}
