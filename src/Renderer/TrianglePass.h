#pragma once
#include "Renderer/FrameContext.h"
#include "Renderer/GpuBuffer.h"
#include "Renderer/RootSignature.h"
#include "Renderer/PipelineState.h"
namespace isr {
class TrianglePass {
public:
    TrianglePass(ID3D12Device*, ID3D12GraphicsCommandList*);
    void Draw(ID3D12GraphicsCommandList*, FrameContext&);
    void FinishUpload() { vertices_->ReleaseUpload(); }
private:
    std::unique_ptr<RootSignature> root_;
    std::unique_ptr<PipelineState> pipeline_;
    std::unique_ptr<GpuBuffer> vertices_;
};
}
