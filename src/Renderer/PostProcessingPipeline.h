#pragma once
#include "Renderer/BloomPass.h"
#include "Renderer/ToneMapPass.h"
namespace isr {
// New HDR effects consume/return PostProcessImage before tone mapping. Outputs
// must use a different target and finish in PIXEL_SHADER_RESOURCE state.
class PostProcessingPipeline {
public:
    PostProcessingPipeline(ID3D12Device*,DescriptorAllocator& resources);
    void Resize(ID3D12Device*,UINT width,UINT height); // GPU must be idle.
    PostProcessImage ProcessHDR(ID3D12GraphicsCommandList*,PostProcessImage,const LookParameters&,RenderMode);
    void Draw(ID3D12GraphicsCommandList*,PostProcessImage,D3D12_CPU_DESCRIPTOR_HANDLE output,const RenderSettings&);
private:
    BloomPass bloom_;
    DescriptorAllocator rtvHeap_;
    PostProcessTarget graded_;
    FullscreenPass grading_;
    ToneMapPass toneMap_;
    UINT width_=0,height_=0;
};
}
