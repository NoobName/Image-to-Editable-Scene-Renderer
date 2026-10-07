#pragma once
#include "Renderer/ImageRelightingRenderer.h"
#include "Renderer/SourceImagePass.h"
#include "ScenePackage/RelightingReliability.h"
#include "Renderer/ImageSpecularPass.h"
#include "Renderer/ImageCastShadowPass.h"
#include "Renderer/ImageFogPass.h"
namespace isr {
// Original RGB is the only appearance source. The derived targets never feed the next edit.
class ImageRelightingComposite {
public:
    ImageRelightingComposite(ID3D12Device*,ID3D12GraphicsCommandList*,SourceImagePass&,ImageRelightingRenderer&,
        const AnalysisTextures* = nullptr,const LightingData* = nullptr,std::shared_ptr<const ProtectionMask> = {},const IntrinsicData* = nullptr,const SourceObservation* = nullptr);
    void FinishUpload();
    void Update(ID3D12GraphicsCommandList*,const RelightingParameters&,bool fitCacheValid=true,FrameContext* = nullptr,const ImageFogParameters& = {});
    void Draw(ID3D12GraphicsCommandList*,D3D12_CPU_DESCRIPTOR_HANDLE,UINT,UINT,UINT,float exposure=0,bool overlay=false,float threshold=.35f,bool atmosphere=true)const;
    void EditProtection(ID3D12Device*,ID3D12GraphicsCommandList*,FrameContext&,const SourceObservation&,const RegionProtectionEdit&);
    PostProcessTarget& Ratio(){return ratio_;} PostProcessTarget& Final(){return fog_?fog_->Final():PreFog();}
    PostProcessTarget& PreFog(){return castShadow_&&castShadow_->Available()?castShadow_->Final():final_;}
    ImageFogPass* Fog(){return fog_.get();}
    PostProcessTarget& Baseline(){return final_;}
    ImageCastShadowPass* CastShadow(){return castShadow_.get();}
    package::Json Report()const;
    uint64_t Updates()const{return updates_+(fog_?fog_->Updates():0);}
    Texture& Quality(size_t i){return quality_.at(i)->Image();}
    Texture& Protection(){return protection_->Image();}
    Texture* SpecularTexture(size_t i){return specular_?&specular_->Image(i):nullptr;}
private:
    struct Constants {float rect[4];UINT sw,sh,aw,ah;float strength,epsilon,minRatio,maxRatio;UINT mode,view,pad0,pad1;
        UINT stability,hasBoundaryMaps,maskWidth,maskHeight;float chromaLimit,relativeDepthEdge,normalCosineEdge;UINT fitCacheValid;
        UINT specularEnabled;float specularStrength,specularMaxDelta;UINT specularAvailable;
        float displayExposure;UINT lowConfidence;float confidenceThreshold;UINT reserved;};
    Constants Data(UINT)const;
    std::array<D3D12_GPU_DESCRIPTOR_HANDLE,15> Views(bool ratio,bool final)const;
    DescriptorAllocator resources_,rtvs_;
    DescriptorAllocation source_,old_,next_,null_;
    DescriptorAllocation normal_,depth_,labels_;
    PostProcessTarget ratio_,final_;
    ImageFullscreenPass ratioPass_,compositePass_,previewPass_;
    ImageRelightingRenderer& shading_;
    UINT sw_,sh_,aw_,ah_;
    bool initialized_=false,withinBudget_=true;
    uint64_t oldUpdates_=0,newUpdates_=0,updates_=0;
    RelightingParameters parameters_;
    std::array<std::unique_ptr<NumericTexture>,2> quality_;
    std::unique_ptr<NumericTexture> protection_;
    package::Json reliabilityMetadata_,protectionMetadata_;
    UINT maskWidth_=1,maskHeight_=1;
    bool hasBoundaryMaps_=false,fitCacheValid_=true;
    bool intrinsicAssisted_=false;
    std::unique_ptr<ImageSpecularPass> specular_;
    std::array<DescriptorAllocation,4> specularViews_;
    std::unique_ptr<ImageCastShadowPass> castShadow_;
    DescriptorAllocation castFinal_;
    std::unique_ptr<ImageFogPass> fog_;
    DescriptorAllocation fogFinal_;
    NumericImage importedProtection_;
    uint64_t protectionRevision_=0;
};
}
