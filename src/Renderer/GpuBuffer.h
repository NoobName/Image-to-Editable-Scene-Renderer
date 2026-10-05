#pragma once
#include "Renderer/UploadBuffer.h"
#include <memory>
#include <span>
namespace isr {
class GpuBuffer {
public:
    GpuBuffer(ID3D12Device*, ID3D12GraphicsCommandList*, std::span<const std::byte> data, D3D12_RESOURCE_STATES finalState);
    ID3D12Resource* Resource() const { return resource_.Get(); }
    // Only after the upload command list's fence has completed.
    void ReleaseUpload() { upload_.reset(); }
    D3D12_VERTEX_BUFFER_VIEW VertexView(UINT stride) const;
    D3D12_INDEX_BUFFER_VIEW IndexView() const;
private:
    ComPtr<ID3D12Resource> resource_;
    std::unique_ptr<UploadBuffer> upload_;
    UINT size_{};
};
}
