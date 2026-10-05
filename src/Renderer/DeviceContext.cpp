#include "Renderer/DeviceContext.h"
#include "Core/Log.h"
#include <d3d12sdklayers.h>
#include <vector>
#include <limits>
namespace isr {
DeviceContext::DeviceContext(bool warp) : event_(CreateEventW(nullptr, FALSE, FALSE, nullptr)) {
    if (!event_.Get()) Check(HRESULT_FROM_WIN32(GetLastError()));
    UINT flags = 0;
#ifdef _DEBUG
    ComPtr<ID3D12Debug> debug;
    Check(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)));
    debug->EnableDebugLayer();
    flags = DXGI_CREATE_FACTORY_DEBUG;
    Log("D3D12 Debug Layer: enabled");
#else
    Log("D3D12 Debug Layer: disabled (Release)");
#endif
    Check(CreateDXGIFactory2(flags, IID_PPV_ARGS(&factory_)));
    ComPtr<IDXGIAdapter1> adapter;
    if (warp) {
        Check(factory_->EnumWarpAdapter(IID_PPV_ARGS(&adapter)));
        Check(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device_)));
    } else {
        for (UINT i = 0; ; ++i) {
            const HRESULT enumeration = factory_->EnumAdapters1(i, &adapter);
            if (enumeration == DXGI_ERROR_NOT_FOUND) break;
            Check(enumeration);
            DXGI_ADAPTER_DESC1 desc{}; Check(adapter->GetDesc1(&desc));
            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) { adapter.Reset(); continue; }
            const HRESULT support = D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device_));
            if (SUCCEEDED(support)) break;
            Log("Adapter does not support D3D12; trying next adapter"); adapter.Reset();
        }
        if (!device_) throw std::runtime_error("No D3D12 hardware adapter. Try --warp.");
    }
    DXGI_ADAPTER_DESC1 desc{}; Check(adapter->GetDesc1(&desc));
    char adapterName[1024]{};
    if (!WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, adapterName, sizeof(adapterName), nullptr, nullptr))
        Check(HRESULT_FROM_WIN32(GetLastError()));
    Log(std::string("Adapter: ") + adapterName);
    D3D12_FEATURE_DATA_SHADER_MODEL shaderModel{D3D_SHADER_MODEL_6_0};
    Check(device_->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL,&shaderModel,sizeof(shaderModel)));
    if (shaderModel.HighestShaderModel < D3D_SHADER_MODEL_6_0) throw std::runtime_error("Shader Model 6.0 is required for DXC shaders");
#ifdef _DEBUG
    Check(device_.As(&infoQueue_));
    Check(infoQueue_->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, IsDebuggerPresent()));
    Check(infoQueue_->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, IsDebuggerPresent()));
#endif
    D3D12_COMMAND_QUEUE_DESC queueDesc{}; queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    Check(device_->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&queue_)));
    Check(device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_)));
    Check(queue_->SetName(L"Graphics queue")); Check(fence_->SetName(L"Graphics fence"));
}
DeviceContext::~DeviceContext() {
    try { Flush(); CheckMessages(); } catch (const std::exception& e) { Log(e.what()); }
    if (infoQueue_) Log("Validation summary: errors=" + std::to_string(errors_) + " warnings=" + std::to_string(warnings_));
    else Log("Validation summary: disabled (Release; messages not collected)");
}
uint64_t DeviceContext::Signal() {
    const auto value = nextFence_++; Check(queue_->Signal(fence_.Get(), value)); return value;
}
uint64_t DeviceContext::Completed() const {
    const auto value = fence_->GetCompletedValue();
    if (value == std::numeric_limits<uint64_t>::max()) {
        Check(device_->GetDeviceRemovedReason());
        throw std::runtime_error("GPU fence reports device removal");
    }
    return value;
}
void DeviceContext::Wait(uint64_t value) {
    if (Completed() >= value) return;
    Check(fence_->SetEventOnCompletion(value, event_.Get()));
    const DWORD result = WaitForSingleObject(event_.Get(), 30000);
    if (result == WAIT_FAILED) Check(HRESULT_FROM_WIN32(GetLastError()));
    if (result != WAIT_OBJECT_0) {
        Check(device_->GetDeviceRemovedReason()); throw std::runtime_error("GPU fence wait timed out");
    }
    (void)Completed();
}
void DeviceContext::Flush() { if (queue_ && fence_) Wait(Signal()); }
void DeviceContext::CheckMessages() {
    if (!infoQueue_) return;
    const auto count = infoQueue_->GetNumStoredMessagesAllowedByRetrievalFilter();
    bool failed = false;
    for (UINT64 i = 0; i < count; ++i) {
        SIZE_T size = 0; Check(infoQueue_->GetMessage(i, nullptr, &size));
        std::vector<uint8_t> bytes(size);
        auto* message = reinterpret_cast<D3D12_MESSAGE*>(bytes.data());
        Check(infoQueue_->GetMessage(i, message, &size));
        Log(std::string("D3D12: ") + message->pDescription);
        if (message->Severity <= D3D12_MESSAGE_SEVERITY_ERROR) { ++errors_; failed = true; }
        if (message->Severity == D3D12_MESSAGE_SEVERITY_WARNING) ++warnings_;
    }
    infoQueue_->ClearStoredMessages();
    if (failed) throw std::runtime_error("D3D12 validation error (see log)");
}
}
