#include "Renderer/FullscreenPass.h"
#include "Renderer/ShaderCompiler.h"
namespace isr {
FullscreenPass::FullscreenPass(ID3D12Device* device,const std::filesystem::path& path,const wchar_t* entry,DXGI_FORMAT format,unsigned count)
    :constantCount_(count){
    if(!count||count>32)throw std::invalid_argument("Fullscreen constants must occupy 1..32 DWORDs");
    D3D12_DESCRIPTOR_RANGE ranges[2]={{D3D12_DESCRIPTOR_RANGE_TYPE_SRV,1,0,0,0},{D3D12_DESCRIPTOR_RANGE_TYPE_SRV,1,1,0,0}};
    D3D12_ROOT_PARAMETER params[3]{};
    for(unsigned i=0;i<2;++i){params[i].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;params[i].DescriptorTable={1,&ranges[i]};params[i].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;}
    params[2].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;params[2].Constants={0,0,count};params[2].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_STATIC_SAMPLER_DESC sampler{};sampler.Filter=D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU=sampler.AddressV=sampler.AddressW=D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.ComparisonFunc=D3D12_COMPARISON_FUNC_ALWAYS;sampler.MaxLOD=D3D12_FLOAT32_MAX;sampler.ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
    root_=std::make_unique<RootSignature>(device,params,D3D12_ROOT_SIGNATURE_FLAG_NONE,std::span(&sampler,1));
    ShaderCompiler compiler;
#ifdef _DEBUG
    constexpr bool debug=true;
#else
    constexpr bool debug=false;
#endif
    auto vs=compiler.Compile(path,L"VSMain",ShaderStage::Vertex,debug),ps=compiler.Compile(path,entry,ShaderStage::Pixel,debug);
    auto desc=PipelineState::GraphicsDefaults();desc.pRootSignature=root_->Get();desc.RasterizerState.CullMode=D3D12_CULL_MODE_NONE;desc.RTVFormats[0]=format;
    desc.VS={vs->GetBufferPointer(),vs->GetBufferSize()};desc.PS={ps->GetBufferPointer(),ps->GetBufferSize()};pipeline_=std::make_unique<PipelineState>(device,desc);
}
void FullscreenPass::Draw(ID3D12GraphicsCommandList* list,D3D12_CPU_DESCRIPTOR_HANDLE output,UINT width,UINT height,
    D3D12_GPU_DESCRIPTOR_HANDLE input,D3D12_GPU_DESCRIPTOR_HANDLE secondary,const void* constants) const {
    const D3D12_VIEWPORT viewport{0,0,float(width),float(height),0,1};const D3D12_RECT scissor{0,0,LONG(width),LONG(height)};
    list->RSSetViewports(1,&viewport);list->RSSetScissorRects(1,&scissor);list->OMSetRenderTargets(1,&output,FALSE,nullptr);
    list->SetGraphicsRootSignature(root_->Get());list->SetPipelineState(pipeline_->Get());
    list->SetGraphicsRootDescriptorTable(0,input);list->SetGraphicsRootDescriptorTable(1,secondary);
    list->SetGraphicsRoot32BitConstants(2,constantCount_,constants,0);
    list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);list->DrawInstanced(3,1,0,0);
}
}
