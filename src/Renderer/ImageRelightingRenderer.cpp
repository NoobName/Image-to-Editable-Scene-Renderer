#include "Renderer/ImageRelightingRenderer.h"
#include "Renderer/ShaderCompiler.h"
#include <cmath>
namespace isr {
namespace {
bool ValidLight(const LightingParameters& light){
    double length=0;for(float v:light.direction){if(!std::isfinite(v))return false;length+=double(v)*v;}
    if(length<1e-12)return false;
    for(float v:{light.directIntensity,light.ambientIntensity})if(!std::isfinite(v)||v<0||v>64)return false;
    for(const auto& color:{light.directColor,light.ambientColor})for(float v:color)if(!std::isfinite(v)||v<0||v>1)return false;
    return true;
}
package::Json LightJson(const LightingParameters& p){return {{"direction",p.direction},{"directColor",p.directColor},{"directIntensity",p.directIntensity},{"ambientColor",p.ambientColor},{"ambientIntensity",p.ambientIntensity}};}
}
ImageRelightingRenderer::ImageRelightingRenderer(ID3D12Device* device,AnalysisTextures& maps)
    :resources_(device,D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,16,true),rtvs_(device,D3D12_DESCRIPTOR_HEAP_TYPE_RTV,2),
     normal_(resources_.Allocate()),validity_(resources_.Allocate()),null_(resources_.Allocate()),position_(resources_.Allocate()),old_(rtvs_,resources_),next_(rtvs_,resources_),
     evaluate_(device,ExecutableDirectory()/"shaders/ImageShading.hlsl",L"PSEvaluate",PostProcessFormat,5,52),
     preview_(device,ExecutableDirectory()/"shaders/ImageShading.hlsl",L"PSPreview",DXGI_FORMAT_R8G8B8A8_UNORM,5,52){
    hasMaps_=maps.Map(1)&&maps.Map(4);
    if(hasMaps_){const auto desc=maps.Map(1)->Image().Resource()->GetDesc();width_=UINT(desc.Width);height_=desc.Height;}
    D3D12_SHADER_RESOURCE_VIEW_DESC desc{};desc.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;desc.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;desc.Texture2D.MipLevels=1;
    desc.Format=DXGI_FORMAT_R32G32B32A32_FLOAT;
    device->CreateShaderResourceView(hasMaps_?maps.Map(1)->Image().Resource():nullptr,&desc,normal_.cpu);
    device->CreateShaderResourceView(nullptr,&desc,null_.cpu);
    hasPosition_=maps.Map(3)!=nullptr;device->CreateShaderResourceView(hasPosition_?maps.Map(3)->Image().Resource():nullptr,&desc,position_.cpu);
    desc.Format=DXGI_FORMAT_R32_UINT;device->CreateShaderResourceView(hasMaps_?maps.Map(4)->Image().Resource():nullptr,&desc,validity_.cpu);
    old_.Resize(device,width_,height_);next_.Resize(device,width_,height_);
}
ImageRelightingRenderer::Constants ImageRelightingRenderer::MakeConstants(const LightingParameters& p,UINT view)const{
    Constants c{};c.width=width_;c.height=height_;c.view=view;c.available=hasMaps_&&ValidLight(p);
    for(size_t i=0;i<3;++i){c.direction[i]=p.direction[i];c.direct[i]=p.directColor[i]*p.directIntensity;c.ambient[i]=p.ambientColor[i]*p.ambientIntensity;}
    return c;
}
void ImageRelightingRenderer::Update(ID3D12GraphicsCommandList* list,const LightingSession& state,const std::vector<ImagePointLight>& points){
    if(points.size()>MaxImagePointLights||!std::all_of(points.begin(),points.end(),[](const auto& p){return p.Valid();}))throw std::invalid_argument("Invalid image point lights");
    const auto target=state.EffectiveTarget();
    ID3D12DescriptorHeap* heaps[]{resources_.Heap()};list->SetDescriptorHeaps(1,heaps);
    // Null output inputs during evaluation: an RTV is never also read through an SRV.
    const std::array views{normal_.gpu,validity_.gpu,null_.gpu,null_.gpu,position_.gpu};
    auto draw=[&](PostProcessTarget& out,const LightingParameters& p,bool includePoints){auto c=MakeConstants(p,0);c.available=c.available&&state.available;
        // Root constants are copied into the command list; no per-drag upload or descriptor allocation.
        if(includePoints&&hasPosition_)for(const auto& point:points)if(point.enabled&&point.intensity>0){
            const auto i=UINT(c.direction[3]++);for(size_t a=0;a<3;++a){c.pointPositionRange[i][a]=point.position[a];c.pointColorIntensity[i][a]=point.color[a];}
            c.pointPositionRange[i][3]=point.range;c.pointColorIntensity[i][3]=point.intensity*state.targetGlobalGain;}
        out.Begin(list);evaluate_.Draw(list,out.Rtv(),width_,height_,views,&c);out.End(list);};
    if(!initialized_||state.sourceRevision!=revision_||state.source!=source_||state.available!=available_){draw(old_,state.source,false);++oldUpdates_;}
    if(!initialized_||target!=target_||state.available!=available_||points!=points_||pointGain_!=state.targetGlobalGain){draw(next_,target,true);++newUpdates_;}
    points_=points;pointGain_=state.targetGlobalGain;
    source_=state.source;target_=target;revision_=state.sourceRevision;available_=state.available;initialized_=true;
}
void ImageRelightingRenderer::Draw(ID3D12GraphicsCommandList* list,D3D12_CPU_DESCRIPTOR_HANDLE out,UINT w,UINT h,UINT sw,UINT sh,UINT view)const{
    if(view>=5)throw std::out_of_range("Unknown calculated shading view");
    ID3D12DescriptorHeap* heaps[]{resources_.Heap()};list->SetDescriptorHeaps(1,heaps);
    auto c=MakeConstants(target_,view);c.available=c.available&&available_&&ValidLight(source_);
    const auto rect=FitSourceImage(sw,sh,w,h);c.rect[0]=rect.x;c.rect[1]=rect.y;c.rect[2]=rect.width;c.rect[3]=rect.height;
    const std::array views{normal_.gpu,validity_.gpu,old_.Image().srv,next_.Image().srv,position_.gpu};preview_.Draw(list,out,w,h,views,&c);
}
package::Json ImageRelightingRenderer::Report()const{return {{"size",{width_,height_}},{"oldUpdates",oldUpdates_},{"newUpdates",newUpdates_},{"sourceRevision",revision_},
    {"available",hasMaps_&&available_&&ValidLight(source_)&&ValidLight(target_)},{"source",LightJson(source_)},{"target",LightJson(target_)},
    {"pointLights",points_.size()},{"pointPositionAvailable",hasPosition_},{"pointLightScope","target-only diffuse, inverse-square smooth finite range; no point shadows/specular"},
    {"units","relative-unit-reflectance-response-pi-absorbed"},{"space","lh-camera"},{"alpha","geometry-validity"}};}
}
