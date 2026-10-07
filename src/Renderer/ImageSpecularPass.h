#pragma once
#include "Renderer/ImageRelightingRenderer.h"
#include "ScenePackage/SpecularHandling.h"
#include "ScenePackage/RelightingParameters.h"
namespace isr {
class ImageSpecularPass {
public:
    ImageSpecularPass(ID3D12Device*,ID3D12GraphicsCommandList*,const AnalysisTextures&,const SourceObservation&);
    void FinishUpload();
    void Update(ID3D12GraphicsCommandList*,const LightingParameters&,const LightingParameters&,const RelightingParameters&);
    Texture& Image(size_t);
    const package::Json& Report()const{return metadata_;}
    bool Available()const{return available_;}
private:
    struct Constants {float direction[4],direct[4];float scale,roughnessScale;UINT available,padding;};
    DescriptorAllocator resources_,rtvs_;
    std::array<DescriptorAllocation,6> inputs_;
    std::unique_ptr<NumericTexture> candidate_,protected_;
    PostProcessTarget old_,next_;
    ImageFullscreenPass pass_;
    package::Json metadata_;
    float scale_=1,previousRoughness_=0;
    bool available_=false,initialized_=false;
    LightingParameters source_,target_;
};
}
