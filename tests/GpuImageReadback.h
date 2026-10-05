#pragma once
#include "Renderer/DeviceContext.h"
#include <DirectXMath.h>
#include <vector>
#include <cstring>

// Read every face/mip, respecting D3D12's row and placement alignment.
// The caller supplies a RGBA32F texture in PIXEL_SHADER_RESOURCE state.
inline std::vector<std::vector<DirectX::XMFLOAT4>> ReadGpuImage(
    isr::DeviceContext& context, ID3D12Resource* texture) {
    using namespace isr;
    auto desc = texture->GetDesc();
    const UINT count = desc.DepthOrArraySize * desc.MipLevels;
    std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> footprints(count);
    UINT64 bytes = 0;
    context.Device()->GetCopyableFootprints(&desc, 0, count, 0, footprints.data(), nullptr, nullptr, &bytes);
    D3D12_RESOURCE_DESC buffer{};
    buffer.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    buffer.Width = bytes; buffer.Height = 1; buffer.DepthOrArraySize = 1;
    buffer.MipLevels = 1; buffer.SampleDesc.Count = 1; buffer.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    D3D12_HEAP_PROPERTIES heap{}; heap.Type = D3D12_HEAP_TYPE_READBACK;
    ComPtr<ID3D12Resource> readback;
    Check(context.Device()->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &buffer,
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readback)));
    ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12GraphicsCommandList> list;
    Check(context.Device()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&allocator)));
    Check(context.Device()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr, IID_PPV_ARGS(&list)));
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition = {texture, D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_SOURCE};
    list->ResourceBarrier(1, &barrier);
    for (UINT sub = 0; sub < count; ++sub) {
        D3D12_TEXTURE_COPY_LOCATION src{}, dst{};
        src.pResource = texture; src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX; src.SubresourceIndex = sub;
        dst.pResource = readback.Get(); dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT; dst.PlacedFootprint = footprints[sub];
        list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
    }
    std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
    list->ResourceBarrier(1, &barrier); Check(list->Close());
    ID3D12CommandList* lists[] = {list.Get()};
    context.Queue()->ExecuteCommandLists(1, lists); context.Flush();
    void* mapped{}; D3D12_RANGE range{0, static_cast<SIZE_T>(bytes)};
    Check(readback->Map(0, &range, &mapped));
    std::vector<std::vector<DirectX::XMFLOAT4>> result(count);
    for (UINT sub = 0; sub < count; ++sub) {
        const auto& f = footprints[sub]; auto& pixels = result[sub];
        pixels.resize(size_t(f.Footprint.Width) * f.Footprint.Height);
        for (UINT y = 0; y < f.Footprint.Height; ++y)
            std::memcpy(pixels.data() + size_t(y) * f.Footprint.Width,
                static_cast<const char*>(mapped) + f.Offset + size_t(y) * f.Footprint.RowPitch,
                size_t(f.Footprint.Width) * sizeof(DirectX::XMFLOAT4));
    }
    range = {0, 0}; readback->Unmap(0, &range); context.CheckMessages();
    return result;
}
