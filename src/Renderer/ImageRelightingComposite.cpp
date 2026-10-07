#include "Renderer/ImageRelightingComposite.h"
#include "Renderer/ShaderCompiler.h"
namespace isr {
ImageRelightingComposite::ImageRelightingComposite(ID3D12Device* device,ID3D12GraphicsCommandList* list,SourceImagePass& source,ImageRelightingRenderer& shading,
    const AnalysisTextures* analysis,const LightingData* lighting,std::shared_ptr<const ProtectionMask> protection,const IntrinsicData* intrinsic,const SourceObservation* observation)
    :resources_(device,D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,18,true),rtvs_(device,D3D12_DESCRIPTOR_HEAP_TYPE_RTV,2),
     source_(resources_.Allocate()),old_(resources_.Allocate()),next_(resources_.Allocate()),null_(resources_.Allocate()),
     normal_(resources_.Allocate()),depth_(resources_.Allocate()),labels_(resources_.Allocate()),ratio_(rtvs_,resources_),final_(rtvs_,resources_),
     ratioPass_(device,ExecutableDirectory()/"shaders/ImageRatio.hlsl",L"PSRatio",PostProcessFormat,15,32),
     compositePass_(device,ExecutableDirectory()/"shaders/ImageRatio.hlsl",L"PSComposite",PostProcessFormat,15,32),
     previewPass_(device,ExecutableDirectory()/"shaders/ImageRatio.hlsl",L"PSPreview",DXGI_FORMAT_R8G8B8A8_UNORM,15,32),shading_(shading){
    const auto desc=source.Image().Resource()->GetDesc();sw_=UINT(desc.Width);sh_=desc.Height;
    // Exceeding the derived-target budget disables composition, never resizes or prevents Source display.
    withinBudget_=uint64_t(sw_)*sh_<=16ull*1024*1024;
    aw_=shading.Old().Width();ah_=shading.Old().Height();
    auto srv=[&](ID3D12Resource* resource,DXGI_FORMAT format,D3D12_CPU_DESCRIPTOR_HANDLE handle){D3D12_SHADER_RESOURCE_VIEW_DESC d{};
        d.Format=format;d.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;d.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;d.Texture2D.MipLevels=1;
        device->CreateShaderResourceView(resource,&d,handle);};
    // Explicit shader decode avoids hardware sRGB approximation in float numerical comparisons.
    srv(source.Image().Resource(),DXGI_FORMAT_R8G8B8A8_UNORM,source_.cpu);
    srv(shading.Old().Image().texture->Resource(),PostProcessFormat,old_.cpu);srv(shading.New().Image().texture->Resource(),PostProcessFormat,next_.cpu);
    srv(nullptr,PostProcessFormat,null_.cpu);ratio_.Resize(device,withinBudget_?sw_:1,withinBudget_?sh_:1);final_.Resize(device,withinBudget_?sw_:1,withinBudget_?sh_:1);
    auto resource=[&](size_t i)->ID3D12Resource*{return analysis&&analysis->Map(i)?analysis->Map(i)->Image().Resource():nullptr;};
    hasBoundaryMaps_=resource(1)&&resource(0)&&resource(5);
    srv(resource(1),PostProcessFormat,normal_.cpu);srv(resource(0),DXGI_FORMAT_R32_FLOAT,depth_.cpu);srv(resource(5),DXGI_FORMAT_R32_UINT,labels_.cpu);
    auto quality=BuildRelightingReliability(analysis?analysis->Data():nullptr,lighting,aw_,ah_,intrinsic);reliabilityMetadata_=std::move(quality.provenance);
    intrinsicAssisted_=intrinsic&&lighting&&lighting->metadata.contains("assistance")&&lighting->metadata["assistance"]["selected"].get<bool>();
    for(size_t i=0;i<2;++i)quality_[i]=std::make_unique<NumericTexture>(device,list,resources_,quality.weights[i]);
    // Full source grid preserves imported nearest-sample semantics exactly, even at label edges.
    // This editable weight costs at most 64 MiB under the existing 16M-pixel derived-target limit.
    maskWidth_=withinBudget_?sw_:1;maskHeight_=withinBudget_?sh_:1;
    importedProtection_.width=maskWidth_;importedProtection_.height=maskHeight_;importedProtection_.format=NumericFormat::Float;
    importedProtection_.bytes.resize(size_t(maskWidth_)*maskHeight_*4);
    if(protection){protectionMetadata_=protection->metadata;const auto& image=protection->image;
        for(UINT y=0;y<maskHeight_;++y)for(UINT x=0;x<maskWidth_;++x){
            const auto ix=std::min(UINT((x+.5)*image.width/maskWidth_),image.width-1),iy=std::min(UINT((y+.5)*image.height/maskHeight_),image.height-1);
            std::memcpy(importedProtection_.bytes.data()+(size_t(y)*maskWidth_+x)*4,image.bytes.data()+(size_t(iy)*image.width+ix)*4,4);}}
    protection_=std::make_unique<NumericTexture>(device,list,resources_,importedProtection_);
    if(analysis&&observation)specular_=std::make_unique<ImageSpecularPass>(device,list,*analysis,*observation);
    for(size_t i=0;i<4;++i){specularViews_[i]=resources_.Allocate();srv(specular_?specular_->Image(i).Resource():nullptr,PostProcessFormat,specularViews_[i].cpu);}
    if(analysis&&observation)castShadow_=std::make_unique<ImageCastShadowPass>(device,list,source.Image(),final_,*analysis,*observation,*protection_);
    castFinal_=resources_.Allocate();srv(Final().Image().texture->Resource(),PostProcessFormat,castFinal_.cpu);
    if(observation)fog_=std::make_unique<ImageFogPass>(device,list,PreFog(),*protection_,*observation);
    fogFinal_=resources_.Allocate();srv(fog_?fog_->Output().Image().texture->Resource():nullptr,PostProcessFormat,fogFinal_.cpu);
}
void ImageRelightingComposite::FinishUpload(){for(auto& q:quality_)q->FinishUpload();protection_->FinishUpload();if(specular_)specular_->FinishUpload();if(castShadow_)castShadow_->FinishUpload();if(fog_)fog_->FinishUpload();}
ImageRelightingComposite::Constants ImageRelightingComposite::Data(UINT view)const{return {{0,0,float(sw_),float(sh_)},sw_,sh_,aw_,ah_,
    parameters_.strength,parameters_.epsilon,parameters_.minRatio,parameters_.maxRatio,UINT(parameters_.colorMode),view,UINT(withinBudget_),UINT(intrinsicAssisted_&&parameters_.intrinsicProtection),
    UINT(parameters_.stability),UINT(hasBoundaryMaps_),maskWidth_,maskHeight_,parameters_.chromaLimit,parameters_.relativeDepthEdge,parameters_.normalCosineEdge,UINT(fitCacheValid_),
    UINT(parameters_.specularEnabled&&fitCacheValid_&&shading_.Report()["available"].get<bool>()),parameters_.specularStrength,parameters_.specularMaxDelta,UINT(specular_&&specular_->Available()),0,0,.35f,0};}
std::array<D3D12_GPU_DESCRIPTOR_HANDLE,15> ImageRelightingComposite::Views(bool ratio,bool final)const{return {source_.gpu,old_.gpu,next_.gpu,
    ratio?ratio_.Image().srv:null_.gpu,final?(fog_&&fog_->Active()?fogFinal_.gpu:castFinal_.gpu):null_.gpu,normal_.gpu,depth_.gpu,labels_.gpu,quality_[0]->View(),quality_[1]->View(),protection_->View(),
    specularViews_[0].gpu,specularViews_[1].gpu,specularViews_[2].gpu,specularViews_[3].gpu};}
void ImageRelightingComposite::Update(ID3D12GraphicsCommandList* list,const RelightingParameters& p,bool fitCacheValid,FrameContext* frame,const ImageFogParameters& fog){
    if(!p.Valid())throw std::invalid_argument("Invalid image ratio parameters");
    if(!fog.Valid())throw std::invalid_argument("Invalid additional image fog parameters");
    const auto report=shading_.Report();const uint64_t oldCount=report["oldUpdates"],newCount=report["newUpdates"];
    if(initialized_&&oldCount==oldUpdates_&&newCount==newUpdates_&&p==parameters_&&fitCacheValid_==fitCacheValid){if(fog_)fog_->Update(list,fog,updates_);return;}
    parameters_=p;fitCacheValid_=fitCacheValid;if(specular_)specular_->Update(list,shading_.Source(),shading_.Target(),p);
    const auto constants=Data(0);ID3D12DescriptorHeap* heaps[]{resources_.Heap()};list->SetDescriptorHeaps(1,heaps);
    const auto ratioViews=Views(false,false);
    ratio_.Begin(list);ratioPass_.Draw(list,ratio_.Rtv(),ratio_.Width(),ratio_.Height(),ratioViews,&constants);ratio_.End(list);
    const auto finalViews=Views(true,false);
    final_.Begin(list);compositePass_.Draw(list,final_.Rtv(),final_.Width(),final_.Height(),finalViews,&constants);final_.End(list);
    if(castShadow_){if(!frame)throw std::invalid_argument("Cast shadow update requires a waited frame context");
        castShadow_->Update(list,*frame,shading_.Source(),shading_.Target(),p,fitCacheValid&&report["available"].get<bool>());}
    oldUpdates_=oldCount;newUpdates_=newCount;initialized_=true;++updates_;
    if(fog_)fog_->Update(list,fog,updates_);
}
void ImageRelightingComposite::EditProtection(ID3D12Device* device,ID3D12GraphicsCommandList* list,FrameContext& frame,const SourceObservation& source,const RegionProtectionEdit& edit){
    if(edit.revision==protectionRevision_)return;
    auto image=importedProtection_;
    for(UINT y=0;y<maskHeight_;++y)for(UINT x=0;x<maskWidth_;++x){
        const auto label=SourceLabel(source,(x+.5f)/maskWidth_,(y+.5f)/maskHeight_);if(!label)continue;
        const auto found=edit.weights.find(*label);if(found==edit.weights.end())continue;
        float value;auto* bytes=image.bytes.data()+(size_t(y)*maskWidth_+x)*4;std::memcpy(&value,bytes,4);
        value=std::max(value,found->second);std::memcpy(bytes,&value,4);
    }
    // No descriptor is overwritten. Queue barriers order old reads, copy and new reads; frame owns upload.
    frame.uploads.push_back(protection_->Update(device,list,image));protectionRevision_=edit.revision;initialized_=false;
    protectionMetadata_["sessionRegionRevision"]=edit.revision;protectionMetadata_["sessionLabels"]=edit.weights.size();
}
void ImageRelightingComposite::Draw(ID3D12GraphicsCommandList* list,D3D12_CPU_DESCRIPTOR_HANDLE out,UINT w,UINT h,UINT view,float exposure,bool overlay,float threshold,bool atmosphere)const{
    if(view>=25)throw std::out_of_range("Unknown ratio/composite view");const auto rect=FitSourceImage(sw_,sh_,w,h);auto c=Data(view);
    c.rect[0]=rect.x;c.rect[1]=rect.y;c.rect[2]=rect.width;c.rect[3]=rect.height;
    c.displayExposure=exposure;c.lowConfidence=UINT(overlay);c.confidenceThreshold=threshold;
    ID3D12DescriptorHeap* heaps[]{resources_.Heap()};list->SetDescriptorHeaps(1,heaps);
    auto views=Views(true,true);if(!atmosphere)views[4]=castFinal_.gpu;previewPass_.Draw(list,out,w,h,views,&c);
}
package::Json ImageRelightingComposite::Report()const{return {{"size",{sw_,sh_}},{"analysisSize",{aw_,ah_}},{"updates",updates_},{"strength",parameters_.strength},
    {"epsilon",parameters_.epsilon},{"epsilonUnits","relative-shading-median-proxy-gauge"},{"ratioClamp",{parameters_.minRatio,parameters_.maxRatio}},
    {"available",withinBudget_},{"reason",withinBudget_?"":"Source exceeds 16M-pixel / 512MiB derived-target budget"},
    {"stability",parameters_.stability},{"chromaLimit",parameters_.chromaLimit},{"relativeDepthEdge",parameters_.relativeDepthEdge},{"normalCosineEdge",parameters_.normalCosineEdge},
    {"fitCacheValid",fitCacheValid_},{"intrinsicAssisted",intrinsicAssisted_},{"intrinsicProtection",parameters_.intrinsicProtection},{"reliability",reliabilityMetadata_},{"protection",protectionMetadata_},
    {"castShadow",castShadow_?castShadow_->Report():package::Json{{"available",false}}},
    {"fog",fog_?fog_->Report():package::Json{{"available",false},{"reason","No source observation"}}},
    {"specular",specular_?specular_->Report():package::Json{{"available",false}}},{"specularEnabled",parameters_.specularEnabled},{"specularStrength",parameters_.specularStrength},{"specularMaxDelta",parameters_.specularMaxDelta},{"specularRoughnessScale",parameters_.specularRoughnessScale},
    {"colorMode",parameters_.colorMode==RatioColorMode::Luminance?"luminance":"bounded-color"},{"exposure",0},{"display","linear-to-srgb-once-no-tone-map"}};}
}
