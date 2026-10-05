#include "Renderer/Texture.h"
namespace isr {
Texture::Texture(ID3D12Device* device, UINT width, UINT height, DXGI_FORMAT format, D3D12_RESOURCE_FLAGS flags,
    D3D12_RESOURCE_STATES initialState, const D3D12_CLEAR_VALUE* clear) : state_(initialState) {
    if (!width || !height) throw std::invalid_argument("Empty texture");
    D3D12_HEAP_PROPERTIES heap{}; heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC desc{}; desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = width; desc.Height = height; desc.DepthOrArraySize = 1; desc.MipLevels = 1;
    desc.Format = format; desc.SampleDesc.Count = 1; desc.Flags = flags;
    Check(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, initialState, clear, IID_PPV_ARGS(&resource_)));
}
void Texture::Transition(ID3D12GraphicsCommandList* list, D3D12_RESOURCE_STATES next) {
    if (next == state_) return;
    D3D12_RESOURCE_BARRIER barrier{}; barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition = {resource_.Get(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, state_, next};
    list->ResourceBarrier(1, &barrier); state_ = next;
}
}
