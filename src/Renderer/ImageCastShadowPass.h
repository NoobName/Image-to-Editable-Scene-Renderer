#pragma once
#include "Renderer/ShadowPass.h"
#include "Renderer/ImageFullscreenPass.h"
#include "Renderer/PostProcessTarget.h"
#include "Renderer/AnalysisTextures.h"
#include "ScenePackage/SourceGeometry.h"
#include "ScenePackage/RelightingParameters.h"
namespace isr {
// Owned by the same PreparedScene as its borrowed observations and baseline target.
// Old/new depth and visibility have separate storage and cache keys.
class ImageCastShadowPass {
public:
    ImageCastShadowPass(ID3D12Device*,ID3D12GraphicsCommandList*,Texture& source,PostProcessTarget& baseline,
        const AnalysisTextures&,const SourceObservation&,NumericTexture& protection);
    void FinishUpload();
    void Update(ID3D12GraphicsCommandList*,FrameContext&,const LightingParameters&,const LightingParameters&,const RelightingParameters&,bool);
    void Draw(ID3D12GraphicsCommandList*,D3D12_CPU_DESCRIPTOR_HANDLE,UINT,UINT,UINT)const;
    bool Available()const{return snapshot_.available;}
    Texture* Capture(size_t);
    PostProcessTarget& Final(){return final_;}
    package::Json Report()const;
private:
    struct VisibilityConstants{DirectX::XMFLOAT4X4 vp;float travel[3];float normalBias;float bias;UINT radius,available,pad;};
    struct Constants{float rect[4];UINT sw,sh,aw,ah;float sourceDirection[3];UINT enabled;float sourceDirect[3];float epsilon;
        float sourceAmbient[3];float strength;float targetDirection[3];float maxDelta;float targetDirect[3];float confidence;
        UINT view,same,maskWidth,maskHeight;};
    Constants Data(UINT)const;
    std::array<D3D12_GPU_DESCRIPTOR_HANDLE,11> Views(bool)const;
    SourceGeometry snapshot_;
    DescriptorAllocator resources_,samplers_,dsv_,rtvs_;
    std::unique_ptr<GpuScene> gpu_;
    std::array<std::unique_ptr<ShadowPass>,2> shadows_;
    std::array<DescriptorAllocation,2> shadowViews_;
    std::array<ShadowFrame,2> shadowFrames_;
    std::array<uint64_t,2> shadowUpdates_{};
    std::array<DescriptorAllocation,4> geometryViews_;
    std::array<DescriptorAllocation,4> inputs_;
    DescriptorAllocation null_;
    std::unique_ptr<NumericTexture> evidence_,residual_;
    PostProcessTarget old_,next_,final_;
    ImageFullscreenPass visibilityPass_,compositePass_,previewPass_;
    LightingParameters source_,target_;
    RelightingParameters parameters_;
    UINT sw_,sh_,aw_,ah_,maskWidth_,maskHeight_;
    bool initialized_=false,cacheValid_=false,lightingValid_=false;
    uint64_t visibilityUpdates_=0;
};
}
