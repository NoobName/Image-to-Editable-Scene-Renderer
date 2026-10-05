#include "Renderer/PostProcessingPipeline.h"
#include "Renderer/ShaderCompiler.h"
namespace isr {
PostProcessingPipeline::PostProcessingPipeline(ID3D12Device* device,DescriptorAllocator& resources)
    :bloom_(device,resources),rtvHeap_(device,D3D12_DESCRIPTOR_HEAP_TYPE_RTV,1),graded_(rtvHeap_,resources),
     grading_(device,ExecutableDirectory()/"shaders/ColorGrading.hlsl",L"PSMain",PostProcessFormat,12),toneMap_(device){}
void PostProcessingPipeline::Resize(ID3D12Device* device,UINT width,UINT height){
    if(!width||!height)throw std::invalid_argument("Post Processing requires a nonzero extent");
    if(width==width_&&height==height_)return;
    bloom_.Resize(device,width,height);graded_.Resize(device,width,height);width_=width;height_=height;
}
PostProcessImage PostProcessingPipeline::ProcessHDR(ID3D12GraphicsCommandList* list,PostProcessImage hdr,const LookParameters& look,RenderMode mode){
    ValidateLookParameters(look);
    if(!width_||!height_)throw std::logic_error("Post Processing has not been resized");
    hdr.texture->Transition(list,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    if(mode!=RenderMode::Final)return hdr;
    const bool bloomEnabled=look.bloomEnabled&&look.bloomIntensity>0;
    if(!bloomEnabled&&look.exposure==0&&look.temperature==0&&look.tint==0&&look.saturation==1&&look.contrast==1&&look.vignetteIntensity==0)return hdr;
    const auto bloom=bloomEnabled?bloom_.Draw(list,hdr,look):hdr; // Always bind a valid SRV.
    const struct Constants {
        UINT width,height;float exposure,bloomIntensity;
        float temperature,tint,saturation,contrast;
        float vignetteIntensity,vignetteRadius,vignetteSoftness,padding;
    } c{width_,height_,look.exposure,bloomEnabled?look.bloomIntensity:0,look.temperature,look.tint,look.saturation,look.contrast,
        look.vignetteIntensity,look.vignetteRadius,look.vignetteSoftness,0};
    static_assert(sizeof(Constants)==48);
    graded_.Begin(list);grading_.Draw(list,graded_.Rtv(),width_,height_,hdr.srv,bloom.srv,&c);graded_.End(list);
    return graded_.Image();
}
void PostProcessingPipeline::Draw(ID3D12GraphicsCommandList* list,PostProcessImage hdr,D3D12_CPU_DESCRIPTOR_HANDLE output,const RenderSettings& settings){
    const auto image=ProcessHDR(list,hdr,settings.look,settings.mode);
    toneMap_.Draw(list,output,width_,height_,image.srv,settings.mode,settings.look);
}
}
