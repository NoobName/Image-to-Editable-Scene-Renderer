#pragma once
#include "Renderer/UploadBuffer.h"
namespace isr {
struct FrameContext {
    explicit FrameContext(ID3D12Device* device) : constants(device,4*1024*1024) {
        Check(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator)));
    }
    void Begin(DeviceContext& context) {
        context.Wait(fenceValue); // Both the command memory and constants are GPU-owned until this completes.
        Check(commandAllocator->Reset()); constants.Reset();
    }
    ComPtr<ID3D12CommandAllocator> commandAllocator;
    uint64_t fenceValue{};
    ConstantBufferAllocator constants;
};
}
