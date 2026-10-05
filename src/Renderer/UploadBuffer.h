#pragma once
#include "Renderer/DeviceContext.h"
#include <cstddef>
#include <cstring>
namespace isr {
class UploadBuffer {
public:
    UploadBuffer(ID3D12Device*, size_t bytes);
    ~UploadBuffer();
    UploadBuffer(const UploadBuffer&) = delete;
    UploadBuffer& operator=(const UploadBuffer&) = delete;
    ID3D12Resource* Resource() const { return resource_.Get(); }
    void Write(size_t offset, const void* data, size_t bytes);
    D3D12_GPU_VIRTUAL_ADDRESS Address(size_t offset = 0) const;
    size_t Size() const { return size_; }
private:
    ComPtr<ID3D12Resource> resource_;
    std::byte* mapped_{};
    size_t size_{};
};
class ConstantBufferAllocator {
public:
    explicit ConstantBufferAllocator(ID3D12Device* device, size_t bytes = 64 * 1024) : buffer_(device, bytes) {}
    // Caller must first wait for the owning frame's fence.
    void Reset() { cursor_ = 0; }
    D3D12_GPU_VIRTUAL_ADDRESS Allocate(const void* data, size_t bytes);
    template<class T> D3D12_GPU_VIRTUAL_ADDRESS Allocate(const T& value) { return Allocate(&value, sizeof(T)); }
private:
    UploadBuffer buffer_;
    size_t cursor_{};
};
}
