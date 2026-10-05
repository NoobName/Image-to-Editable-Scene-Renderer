#include "Renderer/ToneMapPass.h"
#include "Renderer/ShaderCompiler.h"
namespace isr {
ToneMapPass::ToneMapPass(ID3D12Device* device)
    :pass_(device,ExecutableDirectory()/"shaders/ToneMap.hlsl",L"PSMain",DXGI_FORMAT_R8G8B8A8_UNORM,4){}
void ToneMapPass::Draw(ID3D12GraphicsCommandList* list,D3D12_CPU_DESCRIPTOR_HANDLE output,UINT width,UINT height,
    D3D12_GPU_DESCRIPTOR_HANDLE texture,RenderMode mode,const LookParameters& look){
    // Exposure is already applied in the HDR chain. Data views bypass that chain.
    const struct {float exposure;uint32_t mode,tone;float gamma;} constants{
        0,static_cast<uint32_t>(mode),static_cast<uint32_t>(look.toneMapping),look.gamma};
    pass_.Draw(list,output,width,height,texture,texture,&constants);
}
}
