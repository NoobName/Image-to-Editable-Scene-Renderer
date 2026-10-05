#pragma once
#include "Renderer/DeviceContext.h"
#include <filesystem>
namespace isr {
// Diagnostic GPU readback. This path deliberately waits; normal rendering does not.
class FrameCapture {
public:
    FrameCapture(ID3D12Device*, ID3D12GraphicsCommandList*, ID3D12Resource* renderTarget);
    void Save(const std::filesystem::path&) const; // Call only after submission fence completes.
    void LogHdrStatistics() const;
private:
    ComPtr<ID3D12Resource> readback_;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint_{};
    UINT width_{}, height_{};
    UINT64 size_{};
    DXGI_FORMAT format_{};
};
}
