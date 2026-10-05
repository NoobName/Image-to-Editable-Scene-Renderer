#include "Renderer/GpuBuffer.h"
#include <limits>
namespace isr {
GpuBuffer::GpuBuffer(ID3D12Device* device, ID3D12GraphicsCommandList* list, std::span<const std::byte> data, D3D12_RESOURCE_STATES finalState) {
    if (data.empty() || data.size() > std::numeric_limits<UINT>::max()) throw std::invalid_argument("Invalid GPU buffer size");
    size_ = static_cast<UINT>(data.size());
    upload_ = std::make_unique<UploadBuffer>(device, size_); upload_->Write(0, data.data(), size_);
    D3D12_HEAP_PROPERTIES heap{}; heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC desc{}; desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = size_; desc.Height = 1; desc.DepthOrArraySize = 1; desc.MipLevels = 1;
    desc.SampleDesc.Count = 1; desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    // Buffers support implicit COMMON -> COPY_DEST promotion at CopyBufferRegion.
    Check(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&resource_)));
    list->CopyBufferRegion(resource_.Get(), 0, upload_->Resource(), 0, size_);
    D3D12_RESOURCE_BARRIER barrier{}; barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition = {resource_.Get(), D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES, D3D12_RESOURCE_STATE_COPY_DEST, finalState};
    list->ResourceBarrier(1, &barrier);
}
D3D12_VERTEX_BUFFER_VIEW GpuBuffer::VertexView(UINT stride) const {
    if (!stride || size_ % stride) throw std::invalid_argument("Invalid vertex stride");
    return {resource_->GetGPUVirtualAddress(), size_, stride};
}
D3D12_INDEX_BUFFER_VIEW GpuBuffer::IndexView() const { return {resource_->GetGPUVirtualAddress(), size_, DXGI_FORMAT_R32_UINT}; }
}
