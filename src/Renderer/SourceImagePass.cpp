#include "Renderer/SourceImagePass.h"
#include "Renderer/ShaderCompiler.h"
namespace isr {
SourceImagePass::SourceImagePass(ID3D12Device* device,ID3D12GraphicsCommandList* list,DescriptorAllocator& heap,const ImageData& image)
    :texture_(device,image.width,image.height,DXGI_FORMAT_R8G8B8A8_TYPELESS,D3D12_RESOURCE_FLAG_NONE,D3D12_RESOURCE_STATE_COPY_DEST),
     view_(heap.Allocate()),pass_(device,ExecutableDirectory()/"shaders/SourceImage.hlsl",L"PSMain",DXGI_FORMAT_R8G8B8A8_UNORM,8),
     width_(image.width),height_(image.height){
    if(image.rgba.size()!=size_t(width_)*height_*4)throw std::runtime_error("Source image pixel size mismatch");
    const auto desc=texture_.Resource()->GetDesc();D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};UINT64 bytes=0;
    device->GetCopyableFootprints(&desc,0,1,0,&footprint,nullptr,nullptr,&bytes);
    upload_=std::make_unique<UploadBuffer>(device,static_cast<size_t>(bytes));
    for(uint32_t y=0;y<height_;++y)upload_->Write(size_t(footprint.Offset)+size_t(y)*footprint.Footprint.RowPitch,
        image.rgba.data()+size_t(y)*width_*4,size_t(width_)*4);
    D3D12_TEXTURE_COPY_LOCATION source{},target{};source.pResource=upload_->Resource();source.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;source.PlacedFootprint=footprint;
    target.pResource=texture_.Resource();target.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    list->CopyTextureRegion(&target,0,0,0,&source,nullptr);
    texture_.Transition(list,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    D3D12_SHADER_RESOURCE_VIEW_DESC srv{};srv.Format=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;
    srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;srv.Texture2D.MipLevels=1;
    device->CreateShaderResourceView(texture_.Resource(),&srv,view_.cpu);
}
void SourceImagePass::Draw(ID3D12GraphicsCommandList* list,D3D12_CPU_DESCRIPTOR_HANDLE output,uint32_t width,uint32_t height,ImageDebugView view)const{
    const auto rect=FitSourceImage(width_,height_,width,height);
    const struct {float x,y,width,height;float sourceWidth,sourceHeight;uint32_t grid;float padding;} constants{
        rect.x,rect.y,rect.width,rect.height,float(width_),float(height_),view==ImageDebugView::PixelGrid?1u:0u,0};
    pass_.Draw(list,output,width,height,view_.gpu,view_.gpu,&constants);
}
}
