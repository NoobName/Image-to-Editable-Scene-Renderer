#pragma once
#include "Renderer/FullscreenPass.h"
#include "Renderer/PostProcessTarget.h"
#include "Scene/LookParameters.h"
#include <array>
namespace isr {
class BloomPass {
public:
    static constexpr unsigned MaxLevels=5;
    BloomPass(ID3D12Device*,DescriptorAllocator& resources);
    void Resize(ID3D12Device*,UINT width,UINT height);
    PostProcessImage Draw(ID3D12GraphicsCommandList*,PostProcessImage hdr,const LookParameters&);
private:
    DescriptorAllocator rtvHeap_;
    std::array<std::unique_ptr<PostProcessTarget>,MaxLevels> down_,up_;
    FullscreenPass filter_;
    unsigned levels_=0;
    UINT inputWidth_=0,inputHeight_=0;
};
}
