#pragma once
#include "Renderer/DeviceContext.h"
namespace isr {
struct DescriptorAllocation {
    UINT index{}, count{};
    D3D12_CPU_DESCRIPTOR_HANDLE cpu{};
    D3D12_GPU_DESCRIPTOR_HANDLE gpu{};
};
// Persistent linear arena. No individual free: slots stay valid until heap destruction.
// Future recycling must defer reuse until the last referencing GPU fence completes.
class DescriptorAllocator {
public:
    DescriptorAllocator(ID3D12Device*, D3D12_DESCRIPTOR_HEAP_TYPE, UINT capacity, bool shaderVisible = false);
    DescriptorAllocator(const DescriptorAllocator&) = delete;
    DescriptorAllocator& operator=(const DescriptorAllocator&) = delete;
    DescriptorAllocation Allocate(UINT count = 1);
    D3D12_CPU_DESCRIPTOR_HANDLE Cpu(UINT index) const;
    D3D12_GPU_DESCRIPTOR_HANDLE Gpu(UINT index) const;
    ID3D12DescriptorHeap* Heap() const { return heap_.Get(); }
    UINT Capacity() const { return capacity_; }
private:
    ComPtr<ID3D12DescriptorHeap> heap_;
    UINT capacity_{}, used_{}, stride_{};
    bool visible_{};
};
}
