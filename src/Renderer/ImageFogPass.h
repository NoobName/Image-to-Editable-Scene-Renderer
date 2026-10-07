#pragma once
#include "Renderer/ImageFullscreenPass.h"
#include "Renderer/PostProcessTarget.h"
#include "Renderer/NumericTexture.h"
#include "ScenePackage/ImageFog.h"
namespace isr {
// Borrowed baseline/protection share PreparedScene lifetime; descriptors never change during editing.
class ImageFogPass {
public:
    ImageFogPass(ID3D12Device*,ID3D12GraphicsCommandList*,PostProcessTarget&,NumericTexture&,const SourceObservation&);
    void FinishUpload(){distance_->FinishUpload();labels_->FinishUpload();}
    void Update(ID3D12GraphicsCommandList*,const ImageFogParameters&,uint64_t baselineRevision);
    void Draw(ID3D12GraphicsCommandList*,D3D12_CPU_DESCRIPTOR_HANDLE,UINT,UINT,UINT)const;
    PostProcessTarget& Final(){return Active()?final_:baseline_;}
    PostProcessTarget& Output(){return final_;}
    uint64_t Updates()const{return updates_;}
    bool Active()const{return withinBudget_&&data_.available&&parameters_.enabled&&parameters_.density>0&&(!data_.relative||parameters_.allowRelativeScale);}
    Texture& Distance(){return distance_->Image();}
    package::Json Report()const;
private:
    struct Constants{float rect[4];UINT sw,sh,aw,ah;float density,airlight[3];UINT active,view;float maxDistance,pad;};
    Constants Data(UINT)const;
    std::array<D3D12_GPU_DESCRIPTOR_HANDLE,4> Views()const;
    ImageFogData data_;
    DescriptorAllocator resources_,rtvs_;
    DescriptorAllocation baselineView_,protectionView_;
    std::unique_ptr<NumericTexture> distance_,labels_;
    PostProcessTarget final_;
    PostProcessTarget& baseline_;
    ImageFullscreenPass composite_,preview_;
    ImageFogParameters parameters_;
    uint64_t baselineRevision_=UINT64_MAX,updates_=0;
    bool initialized_=false,withinBudget_=false;
};
}
