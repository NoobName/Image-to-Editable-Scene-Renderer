#include "Renderer/NumericTexture.h"
namespace isr {
NumericTexture::NumericTexture(ID3D12Device* device,ID3D12GraphicsCommandList* list,DescriptorAllocator& heap,const NumericImage& image)
    :texture_(device,image.width,image.height,static_cast<DXGI_FORMAT>(image.format),D3D12_RESOURCE_FLAG_NONE,D3D12_RESOURCE_STATE_COPY_DEST),view_(heap.Allocate()){
    if(image.bytes.size()!=size_t(image.width)*image.height*image.Channels()*4)throw std::runtime_error("Numeric upload byte count mismatch");
    const auto desc=texture_.Resource()->GetDesc();D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp{};UINT64 bytes;
    device->GetCopyableFootprints(&desc,0,1,0,&fp,nullptr,nullptr,&bytes);upload_=std::make_unique<UploadBuffer>(device,size_t(bytes));
    const size_t row=size_t(image.width)*image.Channels()*4;
    for(UINT y=0;y<image.height;++y)upload_->Write(size_t(fp.Offset)+size_t(y)*fp.Footprint.RowPitch,image.bytes.data()+y*row,row);
    D3D12_TEXTURE_COPY_LOCATION from{},to{};from.pResource=upload_->Resource();from.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;from.PlacedFootprint=fp;
    to.pResource=texture_.Resource();to.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;list->CopyTextureRegion(&to,0,0,0,&from,nullptr);
    texture_.Transition(list,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    D3D12_SHADER_RESOURCE_VIEW_DESC srv{};srv.Format=desc.Format;srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;
    srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;srv.Texture2D.MipLevels=1;
    device->CreateShaderResourceView(texture_.Resource(),&srv,view_.cpu);
}
}
