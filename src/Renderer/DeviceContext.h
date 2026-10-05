#pragma once
#include "Core/Error.h"
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <cstdint>
namespace isr {
using Microsoft::WRL::ComPtr;
class DeviceContext {
public:
    explicit DeviceContext(bool warp = false);
    ~DeviceContext();
    DeviceContext(const DeviceContext&) = delete;
    DeviceContext& operator=(const DeviceContext&) = delete;
    ID3D12Device* Device() const { return device_.Get(); }
    IDXGIFactory4* Factory() const { return factory_.Get(); }
    ID3D12CommandQueue* Queue() const { return queue_.Get(); }
    uint64_t Signal();
    void Wait(uint64_t value);
    void Flush();
    void CheckMessages();
    uint64_t Completed() const;
    uint64_t Warnings() const { return warnings_; }
private:
    ComPtr<IDXGIFactory4> factory_;
    ComPtr<ID3D12Device> device_;
    ComPtr<ID3D12CommandQueue> queue_;
    ComPtr<ID3D12Fence> fence_;
    ComPtr<ID3D12InfoQueue> infoQueue_;
    UniqueHandle event_;
    uint64_t nextFence_ = 1;
    uint64_t warnings_ = 0, errors_ = 0;
    uint64_t readMessages_ = 0;
};
}
