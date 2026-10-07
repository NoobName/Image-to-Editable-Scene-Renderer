#include "Renderer/ImageSpecularPass.h"
#include "Renderer/ShaderCompiler.h"
namespace isr {
ImageSpecularPass::ImageSpecularPass(ID3D12Device* device,ID3D12GraphicsCommandList* list,const AnalysisTextures& maps,const SourceObservation& source)
    :resources_(device,D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,16,true),rtvs_(device,D3D12_DESCRIPTOR_HEAP_TYPE_RTV,2),
    old_(rtvs_,resources_),next_(rtvs_,resources_),pass_(device,ExecutableDirectory()/"shaders/ImageSpecular.hlsl",L"PSEvaluate",PostProcessFormat,6,12){
    auto evidence=BuildSpecularEvidence(source);metadata_=std::move(evidence.metadata);scale_=evidence.scale;available_=evidence.available;
    constexpr size_t indices[]{1,3,6,7,8,4};
    for(size_t i=0;i<6;++i){inputs_[i]=resources_.Allocate();auto* texture=maps.Map(indices[i]);D3D12_SHADER_RESOURCE_VIEW_DESC d{};
        d.Format=i<3?PostProcessFormat:i==5?DXGI_FORMAT_R32_UINT:DXGI_FORMAT_R32_FLOAT;d.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;
        d.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;d.Texture2D.MipLevels=1;
        device->CreateShaderResourceView(texture?texture->Image().Resource():nullptr,&d,inputs_[i].cpu);}
    old_.Resize(device,evidence.candidate.width,evidence.candidate.height);next_.Resize(device,evidence.candidate.width,evidence.candidate.height);
    candidate_=std::make_unique<NumericTexture>(device,list,resources_,evidence.candidate);
    protected_=std::make_unique<NumericTexture>(device,list,resources_,evidence.protectedResidual);
}
void ImageSpecularPass::FinishUpload(){candidate_->FinishUpload();protected_->FinishUpload();}
Texture& ImageSpecularPass::Image(size_t i){if(i==0)return *old_.Image().texture;if(i==1)return *next_.Image().texture;if(i==2)return candidate_->Image();if(i==3)return protected_->Image();throw std::out_of_range("Specular map index");}
void ImageSpecularPass::Update(ID3D12GraphicsCommandList* list,const LightingParameters& source,const LightingParameters& target,const RelightingParameters& parameters){
    ID3D12DescriptorHeap* heaps[]{resources_.Heap()};list->SetDescriptorHeaps(1,heaps);std::array<D3D12_GPU_DESCRIPTOR_HANDLE,6> views{};
    for(size_t i=0;i<6;++i)views[i]=inputs_[i].gpu;
    auto draw=[&](PostProcessTarget& out,const LightingParameters& p,float rough){Constants c{};c.scale=scale_;c.roughnessScale=rough;c.available=available_;
        for(unsigned i=0;i<3;++i){c.direction[i]=p.direction[i];c.direct[i]=p.directColor[i]*p.directIntensity;}
        out.Begin(list);pass_.Draw(list,out.Rtv(),out.Width(),out.Height(),views,&c);out.End(list);};
    if(!initialized_||source!=source_)draw(old_,source,1);
    if(!initialized_||target!=target_||parameters.specularRoughnessScale!=previousRoughness_)draw(next_,target,parameters.specularRoughnessScale);
    source_=source;target_=target;previousRoughness_=parameters.specularRoughnessScale;initialized_=true;
}
}
