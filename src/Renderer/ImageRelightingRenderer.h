#pragma once
#include "Renderer/AnalysisTextures.h"
#include "Renderer/ImageFullscreenPass.h"
#include "Renderer/PostProcessTarget.h"
namespace isr {
// Owns derived image responses, not the fixed analysis textures. Both are retired as one scene.
class ImageRelightingRenderer {
public:
    ImageRelightingRenderer(ID3D12Device*,AnalysisTextures&);
    void Update(ID3D12GraphicsCommandList*,const LightingSession&);
    void Draw(ID3D12GraphicsCommandList*,D3D12_CPU_DESCRIPTOR_HANDLE,UINT,UINT,UINT,UINT,UINT)const;
    PostProcessTarget& Old(){return old_;} PostProcessTarget& New(){return next_;}
    package::Json Report()const;
    const LightingParameters& Source()const{return source_;}
    const LightingParameters& Target()const{return target_;}
private:
    struct Constants {float rect[4];UINT width,height,view,available;float direction[4],direct[4],ambient[4];};
    Constants MakeConstants(const LightingParameters&,UINT)const;
    DescriptorAllocator resources_,rtvs_;
    DescriptorAllocation normal_,validity_,null_;
    PostProcessTarget old_,next_;
    ImageFullscreenPass evaluate_,preview_;
    bool hasMaps_=false,available_=false,initialized_=false;
    UINT width_=1,height_=1;
    uint64_t revision_=0,oldUpdates_=0,newUpdates_=0;
    LightingParameters source_,target_;
};
}
