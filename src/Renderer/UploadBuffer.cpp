#include "Renderer/UploadBuffer.h"
namespace isr {
UploadBuffer::UploadBuffer(ID3D12Device* device, size_t bytes) : size_(bytes) {
    if (!bytes) throw std::invalid_argument("Empty upload buffer");
    D3D12_HEAP_PROPERTIES heap{}; heap.Type = D3D12_HEAP_TYPE_UPLOAD;
    D3D12_RESOURCE_DESC desc{}; desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = bytes; desc.Height = 1; desc.DepthOrArraySize = 1; desc.MipLevels = 1;
    desc.SampleDesc.Count = 1; desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    Check(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr, IID_PPV_ARGS(&resource_)));
    const D3D12_RANGE readRange{0, 0};
    Check(resource_->Map(0, &readRange, reinterpret_cast<void**>(&mapped_)));
}
UploadBuffer::~UploadBuffer() { if (mapped_) resource_->Unmap(0, nullptr); }
void UploadBuffer::Write(size_t offset, const void* data, size_t bytes) {
    if (!data || offset > size_ || bytes > size_ - offset) throw std::out_of_range("Upload write overflow");
    std::memcpy(mapped_ + offset, data, bytes);
}
D3D12_GPU_VIRTUAL_ADDRESS UploadBuffer::Address(size_t offset) const {
    if (offset >= size_) throw std::out_of_range("Upload address overflow");
    return resource_->GetGPUVirtualAddress() + offset;
}
D3D12_GPU_VIRTUAL_ADDRESS ConstantBufferAllocator::Allocate(const void* data, size_t bytes) {
    if (!bytes || bytes > buffer_.Size()) throw std::out_of_range("Invalid constant allocation");
    const size_t aligned = (bytes + 255) & ~size_t(255);
    if (aligned > buffer_.Size() - cursor_) throw std::out_of_range("Frame constant buffer exhausted");
    buffer_.Write(cursor_, data, bytes); const auto address = buffer_.Address(cursor_); cursor_ += aligned; return address;
}
}
