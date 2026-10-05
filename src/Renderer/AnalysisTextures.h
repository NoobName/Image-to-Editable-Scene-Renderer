#pragma once
#include "Renderer/NumericTexture.h"
#include "Renderer/RootSignature.h"
#include "Renderer/PipelineState.h"
#include "ScenePackage/RelightingSession.h"
namespace isr {
class AnalysisTextures {
public:
    AnalysisTextures(ID3D12Device*,ID3D12GraphicsCommandList*,DescriptorAllocator&,std::shared_ptr<const AnalysisMaps>);
    void FinishUpload();
    void Draw(ID3D12GraphicsCommandList*,D3D12_CPU_DESCRIPTOR_HANDLE,uint32_t vw,uint32_t vh,uint32_t sw,uint32_t sh,size_t selected,bool raw=false)const;
    NumericTexture* Map(size_t i)const{return maps_.at(i).get();}
    const AnalysisMaps* Data()const{return data_.get();}
private:
    std::shared_ptr<const AnalysisMaps> data_;
    std::array<std::unique_ptr<NumericTexture>,13> maps_;
    DescriptorAllocation nullFloat_,nullUint_;
    std::unique_ptr<RootSignature> root_;
    std::unique_ptr<PipelineState> pipeline_,rawPipeline_;
};
}
