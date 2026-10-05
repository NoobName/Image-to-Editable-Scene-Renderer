#pragma once
#include "Renderer/DeviceContext.h"
#include <filesystem>
#include "Scene/TextureAsset.h"
namespace isr {
// Diagnostic GPU readback. This path deliberately waits; normal rendering does not.
class FrameCapture {
public:
    FrameCapture(ID3D12Device*, ID3D12GraphicsCommandList*, ID3D12Resource* renderTarget);
    void Save(const std::filesystem::path&) const; // Call only after submission fence completes.
    void LogHdrStatistics() const;
    unsigned CompareRgb(const ImageData&,unsigned tolerance=1)const; // Native-size encoded RGB8 comparison after fence.
private:
    ComPtr<ID3D12Resource> readback_;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint_{};
    UINT width_{}, height_{};
    UINT64 size_{};
    DXGI_FORMAT format_{};
};
}
