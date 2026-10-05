#include "Renderer/RootSignature.h"
#include "Core/Log.h"
namespace isr {
RootSignature::RootSignature(ID3D12Device* device, std::span<const D3D12_ROOT_PARAMETER> parameters, D3D12_ROOT_SIGNATURE_FLAGS flags,
    std::span<const D3D12_STATIC_SAMPLER_DESC> samplers) {
    D3D12_ROOT_SIGNATURE_DESC desc{}; desc.NumParameters = static_cast<UINT>(parameters.size());
    desc.pParameters = parameters.data(); desc.Flags = flags;
    desc.NumStaticSamplers=static_cast<UINT>(samplers.size());desc.pStaticSamplers=samplers.data();
    ComPtr<ID3DBlob> blob, errors;
    const HRESULT result = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &errors);
    if (errors) Log(std::string_view(static_cast<const char*>(errors->GetBufferPointer()), errors->GetBufferSize()));
    Check(result);
    Check(device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&signature_)));
}
}
