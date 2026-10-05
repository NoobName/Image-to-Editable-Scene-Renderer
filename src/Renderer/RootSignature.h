#pragma once
#include "Renderer/DeviceContext.h"
#include <span>
namespace isr {
class RootSignature {
public:
    RootSignature(ID3D12Device*, std::span<const D3D12_ROOT_PARAMETER>, D3D12_ROOT_SIGNATURE_FLAGS flags,
        std::span<const D3D12_STATIC_SAMPLER_DESC> samplers = {});
    ID3D12RootSignature* Get() const { return signature_.Get(); }
private:
    ComPtr<ID3D12RootSignature> signature_;
};
}
