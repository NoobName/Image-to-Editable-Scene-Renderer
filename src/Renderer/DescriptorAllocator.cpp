#include "Renderer/DescriptorAllocator.h"
namespace isr {
DescriptorAllocator::DescriptorAllocator(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, UINT capacity, bool visible)
    : capacity_(capacity), visible_(visible) {
    if (!capacity || (visible && type != D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV && type != D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER))
        throw std::invalid_argument("Invalid descriptor heap configuration");
    D3D12_DESCRIPTOR_HEAP_DESC desc{}; desc.Type = type; desc.NumDescriptors = capacity;
    desc.Flags = visible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    Check(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap_)));
    stride_ = device->GetDescriptorHandleIncrementSize(type);
}
DescriptorAllocation DescriptorAllocator::Allocate(UINT count) {
    if (!count || count > capacity_ - used_) throw std::out_of_range("Descriptor heap exhausted");
    const UINT index = used_; used_ += count;
    return {index, count, Cpu(index), visible_ ? Gpu(index) : D3D12_GPU_DESCRIPTOR_HANDLE{}};
}
D3D12_CPU_DESCRIPTOR_HANDLE DescriptorAllocator::Cpu(UINT index) const {
    if (index >= used_) throw std::out_of_range("Descriptor was not allocated");
    auto handle = heap_->GetCPUDescriptorHandleForHeapStart(); handle.ptr += SIZE_T(index) * stride_; return handle;
}
D3D12_GPU_DESCRIPTOR_HANDLE DescriptorAllocator::Gpu(UINT index) const {
    if (!visible_ || index >= used_) throw std::out_of_range("No GPU descriptor handle");
    auto handle = heap_->GetGPUDescriptorHandleForHeapStart(); handle.ptr += UINT64(index) * stride_; return handle;
}
}
