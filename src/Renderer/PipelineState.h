#pragma once
#include "Renderer/DeviceContext.h"
namespace isr {
class PipelineState {
public:
    PipelineState(ID3D12Device* device, const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc) {
        Check(device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&state_)));
    }
    PipelineState(ID3D12Device* device, const D3D12_COMPUTE_PIPELINE_STATE_DESC& desc) {
        Check(device->CreateComputePipelineState(&desc, IID_PPV_ARGS(&state_)));
    }
    ID3D12PipelineState* Get() const { return state_.Get(); }
    static D3D12_GRAPHICS_PIPELINE_STATE_DESC GraphicsDefaults();
private:
    ComPtr<ID3D12PipelineState> state_;
};
}
