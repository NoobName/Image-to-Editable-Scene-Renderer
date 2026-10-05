#include "Renderer/TextureReadback.h"
#include "Core/Log.h"
#include <cstring>
#include <cmath>
namespace isr {
TextureReadback::TextureReadback(ID3D12Device* device,ID3D12GraphicsCommandList* list,Texture& texture):before_(texture.State()){
    const auto desc=texture.Resource()->GetDesc();layout_.width=UINT(desc.Width);layout_.height=desc.Height;layout_.format=static_cast<NumericFormat>(desc.Format);
    if(desc.Format!=DXGI_FORMAT_R32_FLOAT&&desc.Format!=DXGI_FORMAT_R32G32B32A32_FLOAT&&desc.Format!=DXGI_FORMAT_R32_UINT)throw std::runtime_error("Numeric readback unsupported format");
    device->GetCopyableFootprints(&desc,0,1,0,&footprint_,nullptr,nullptr,&bytes_);
    D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_READBACK;D3D12_RESOURCE_DESC rd{};rd.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;rd.Width=bytes_;
    rd.Height=1;rd.DepthOrArraySize=1;rd.MipLevels=1;rd.SampleDesc.Count=1;rd.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    Check(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&rd,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&buffer_)));
    texture.Transition(list,D3D12_RESOURCE_STATE_COPY_SOURCE);
    D3D12_TEXTURE_COPY_LOCATION from{},to{};from.pResource=texture.Resource();from.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    to.pResource=buffer_.Get();to.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;to.PlacedFootprint=footprint_;list->CopyTextureRegion(&to,0,0,0,&from,nullptr);
    texture.Transition(list,before_);
    Log("Numeric readback states: before="+std::to_string(before_)+" capture=COPY_SOURCE(2048) after="+std::to_string(texture.State()));
}
NumericImage TextureReadback::Read()const{
    auto result=layout_;const size_t row=size_t(result.width)*result.Channels()*4;result.bytes.resize(row*result.height);
    void* mapped{};const D3D12_RANGE range{0,size_t(bytes_)};Check(buffer_->Map(0,&range,&mapped));
    for(UINT y=0;y<result.height;++y)std::memcpy(result.bytes.data()+y*row,static_cast<uint8_t*>(mapped)+footprint_.Offset+size_t(y)*footprint_.Footprint.RowPitch,row);
    const D3D12_RANGE written{0,0};buffer_->Unmap(0,&written);
    if(result.format!=NumericFormat::Label)for(size_t p=0;p<size_t(result.width)*result.height;++p)for(unsigned c=0;c<result.Channels();++c)
        if(!std::isfinite(result.FloatAt(p,c)))throw std::runtime_error("Numeric GPU readback contains NaN/Inf");
    return result;
}
}
