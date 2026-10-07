#include "Renderer/ImageFogPass.h"
#include "Renderer/ShaderCompiler.h"
#include "ScenePackage/RelightingSession.h"
namespace isr {
ImageFogPass::ImageFogPass(ID3D12Device* device,ID3D12GraphicsCommandList* list,PostProcessTarget& baseline,NumericTexture& protection,const SourceObservation& source)
    :data_(BuildImageFogData(source)),resources_(device,D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,5,true),rtvs_(device,D3D12_DESCRIPTOR_HEAP_TYPE_RTV,1),
    baselineView_(resources_.Allocate()),protectionView_(resources_.Allocate()),final_(rtvs_,resources_),baseline_(baseline),
    composite_(device,ExecutableDirectory()/"shaders/ImageFog.hlsl",L"PSComposite",PostProcessFormat,4,16),
    preview_(device,ExecutableDirectory()/"shaders/ImageFog.hlsl",L"PSPreview",DXGI_FORMAT_R8G8B8A8_UNORM,4,16){
    auto srv=[&](Texture& t,D3D12_CPU_DESCRIPTOR_HANDLE handle,DXGI_FORMAT format){D3D12_SHADER_RESOURCE_VIEW_DESC d{};d.Format=format;
        d.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;d.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;d.Texture2D.MipLevels=1;device->CreateShaderResourceView(t.Resource(),&d,handle);};
    srv(*baseline.Image().texture,baselineView_.cpu,PostProcessFormat);srv(protection.Image(),protectionView_.cpu,DXGI_FORMAT_R32_FLOAT);
    distance_=std::make_unique<NumericTexture>(device,list,resources_,data_.distance);
    NumericImage labels{data_.distance.width,data_.distance.height,NumericFormat::Label,std::vector<uint8_t>(size_t(data_.distance.width)*data_.distance.height*4)};
    if(data_.available)labels=*source.analysisMaps->maps[5].image;
    labels_=std::make_unique<NumericTexture>(device,list,resources_,labels);
    // One extra RGBA32 target, at most 128 MiB. Above 8M source pixels, explicitly disable fog.
    const auto size=source.anchor->sourceSize;withinBudget_=uint64_t(size[0])*size[1]<=8ull*1024*1024&&baseline.Width()==size[0]&&baseline.Height()==size[1];
    final_.Resize(device,withinBudget_?size[0]:1,withinBudget_?size[1]:1);
}
ImageFogPass::Constants ImageFogPass::Data(UINT view)const{return {{0,0,float(baseline_.Width()),float(baseline_.Height())},baseline_.Width(),baseline_.Height(),
    data_.distance.width,data_.distance.height,parameters_.density,{parameters_.airlight[0],parameters_.airlight[1],parameters_.airlight[2]},UINT(Active()),view,
    float(data_.report.value("maxDistance",1.0)),0};}
std::array<D3D12_GPU_DESCRIPTOR_HANDLE,4> ImageFogPass::Views()const{return {baselineView_.gpu,distance_->View(),labels_->View(),protectionView_.gpu};}
void ImageFogPass::Update(ID3D12GraphicsCommandList* list,const ImageFogParameters& p,uint64_t revision){
    if(!p.Valid())throw std::invalid_argument("Invalid additional image fog parameters");
    if(initialized_&&p==parameters_&&revision==baselineRevision_)return;
    parameters_=p;baselineRevision_=revision;initialized_=true;
    if(!Active())return;ID3D12DescriptorHeap* heaps[]{resources_.Heap()};list->SetDescriptorHeaps(1,heaps);
    auto c=Data(0);auto views=Views();final_.Begin(list);composite_.Draw(list,final_.Rtv(),final_.Width(),final_.Height(),views,&c);final_.End(list);++updates_;
}
void ImageFogPass::Draw(ID3D12GraphicsCommandList* list,D3D12_CPU_DESCRIPTOR_HANDLE out,UINT w,UINT h,UINT view)const{
    const auto r=FitSourceImage(baseline_.Width(),baseline_.Height(),w,h);auto c=Data(view);c.rect[0]=r.x;c.rect[1]=r.y;c.rect[2]=r.width;c.rect[3]=r.height;
    ID3D12DescriptorHeap* heaps[]{resources_.Heap()};list->SetDescriptorHeaps(1,heaps);const auto views=Views();preview_.Draw(list,out,w,h,views,&c);
}
package::Json ImageFogPass::Report()const{auto j=data_.report;j["withinBudget"]=withinBudget_;j["active"]=Active();j["enabled"]=parameters_.enabled;
    j["allowRelativeScale"]=parameters_.allowRelativeScale;j["density"]=parameters_.density;j["airlightLinear"]=parameters_.airlight;j["updates"]=updates_;
    j["order"]="linear relighting + specular + cast -> additional fog -> display exposure once -> sRGB encode";
    j["targetBudgetMiB"]=128;j["maxSourcePixels"]=8*1024*1024;
    if(!withinBudget_)j["reason"]="Fog unavailable: source exceeds 8M pixels or native baseline unavailable; no downscale";return j;}
}
