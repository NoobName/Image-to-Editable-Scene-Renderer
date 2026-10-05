#include "Renderer/FrameCapture.h"
#include "Core/Log.h"
#include <fstream>
#include <vector>
#include <DirectXPackedVector.h>
#include <cmath>
#include <algorithm>
namespace isr {
FrameCapture::FrameCapture(ID3D12Device* device, ID3D12GraphicsCommandList* list, ID3D12Resource* target) {
    const auto sourceDesc = target->GetDesc(); width_ = static_cast<UINT>(sourceDesc.Width); height_ = sourceDesc.Height;
    format_=sourceDesc.Format;
    device->GetCopyableFootprints(&sourceDesc,0,1,0,&footprint_,nullptr,nullptr,&size_);
    D3D12_HEAP_PROPERTIES heap{}; heap.Type = D3D12_HEAP_TYPE_READBACK;
    D3D12_RESOURCE_DESC desc{}; desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; desc.Width = size_;
    desc.Height = 1; desc.DepthOrArraySize = 1; desc.MipLevels = 1; desc.SampleDesc.Count = 1; desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    Check(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&readback_)));
    D3D12_RESOURCE_BARRIER barrier{}; barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition = {target,D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_COPY_SOURCE};
    list->ResourceBarrier(1,&barrier);
    D3D12_TEXTURE_COPY_LOCATION destination{}; destination.pResource = readback_.Get();
    destination.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT; destination.PlacedFootprint = footprint_;
    D3D12_TEXTURE_COPY_LOCATION source{}; source.pResource = target; source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    list->CopyTextureRegion(&destination,0,0,0,&source,nullptr);
    std::swap(barrier.Transition.StateBefore,barrier.Transition.StateAfter); list->ResourceBarrier(1,&barrier);
}
void FrameCapture::Save(const std::filesystem::path& path) const {
    if(format_!=DXGI_FORMAT_R8G8B8A8_UNORM)throw std::runtime_error("BMP capture requires RGBA8");
    if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path());
    std::vector<unsigned char> pixels(size_t(width_)*height_*4);
    void* mapped{}; const D3D12_RANGE read{0,static_cast<SIZE_T>(size_)}; Check(readback_->Map(0,&read,&mapped));
    const auto* source = static_cast<const unsigned char*>(mapped) + footprint_.Offset;
    for (UINT y = 0; y < height_; ++y) for (UINT x = 0; x < width_; ++x) {
        const auto src = source + size_t(y)*footprint_.Footprint.RowPitch + size_t(x)*4;
        auto* dst = pixels.data() + (size_t(y)*width_+x)*4;
        dst[0]=src[2]; dst[1]=src[1]; dst[2]=src[0]; dst[3]=255;
    }
    const D3D12_RANGE written{0,0}; readback_->Unmap(0,&written);
    BITMAPFILEHEADER file{}; file.bfType = 0x4D42; file.bfOffBits = sizeof(file)+sizeof(BITMAPINFOHEADER);
    file.bfSize = file.bfOffBits + static_cast<DWORD>(pixels.size());
    BITMAPINFOHEADER info{}; info.biSize = sizeof(info); info.biWidth = static_cast<LONG>(width_);
    info.biHeight = -static_cast<LONG>(height_); info.biPlanes = 1; info.biBitCount = 32; info.biCompression = BI_RGB;
    std::ofstream out(path,std::ios::binary); out.exceptions(std::ios::badbit | std::ios::failbit);
    out.write(reinterpret_cast<const char*>(&file),sizeof(file)); out.write(reinterpret_cast<const char*>(&info),sizeof(info));
    out.write(reinterpret_cast<const char*>(pixels.data()),static_cast<std::streamsize>(pixels.size()));
    Log("Saved GPU frame capture: " + path.string());
}
void FrameCapture::LogHdrStatistics() const {
    const bool fullPrecision=format_==DXGI_FORMAT_R32G32B32A32_FLOAT;
    if(!fullPrecision&&format_!=DXGI_FORMAT_R16G16B16A16_FLOAT)throw std::runtime_error("HDR statistics require RGBA16F/32F");
    void* mapped{};const D3D12_RANGE read{0,static_cast<SIZE_T>(size_)};Check(readback_->Map(0,&read,&mapped));
    float maximum=0;size_t nonfinite=0;double total=0;
    for(UINT y=0;y<height_;++y){const auto* row=reinterpret_cast<const uint16_t*>(static_cast<const uint8_t*>(mapped)+footprint_.Offset+size_t(y)*footprint_.Footprint.RowPitch);
        for(UINT x=0;x<width_;++x)for(UINT c=0;c<3;++c){float v=fullPrecision?reinterpret_cast<const float*>(row)[x*4+c]:DirectX::PackedVector::XMConvertHalfToFloat(row[x*4+c]);
            if(!std::isfinite(v))++nonfinite;else {maximum=std::max(maximum,v);total+=v;}}}
    const D3D12_RANGE written{0,0};readback_->Unmap(0,&written);
    Log("HDR statistics: max="+std::to_string(maximum)+" mean="+std::to_string(total/(width_*height_*3))+" nonfinite="+std::to_string(nonfinite));
    if(nonfinite)throw std::runtime_error("Nonfinite HDR pixel detected");
}
}
