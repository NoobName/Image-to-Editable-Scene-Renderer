#include "Renderer/SkyPass.h"
#include "Renderer/ShaderCompiler.h"
#include "Renderer/EnvironmentBaker.h"
namespace isr {
SkyPass::SkyPass(ID3D12Device* device){
    D3D12_DESCRIPTOR_RANGE range{D3D12_DESCRIPTOR_RANGE_TYPE_SRV,1,0,0,0};D3D12_ROOT_PARAMETER params[2]{};
    params[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_CBV;params[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;params[1].DescriptorTable={1,&range};params[1].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_STATIC_SAMPLER_DESC sampler{};sampler.Filter=D3D12_FILTER_MIN_MAG_MIP_LINEAR;sampler.AddressU=sampler.AddressV=sampler.AddressW=D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.ComparisonFunc=D3D12_COMPARISON_FUNC_NONE;sampler.MaxLOD=D3D12_FLOAT32_MAX;sampler.ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
    root_=std::make_unique<RootSignature>(device,params,D3D12_ROOT_SIGNATURE_FLAG_NONE,std::span(&sampler,1));ShaderCompiler compiler;
#ifdef _DEBUG
    constexpr bool debug=true;
#else
    constexpr bool debug=false;
#endif
    auto path=ExecutableDirectory()/"shaders/Sky.hlsl";auto vs=compiler.Compile(path,L"VSMain",ShaderStage::Vertex,debug),ps=compiler.Compile(path,L"PSMain",ShaderStage::Pixel,debug);
    auto desc=PipelineState::GraphicsDefaults();desc.pRootSignature=root_->Get();desc.RTVFormats[0]=EnvironmentFormat;desc.RasterizerState.CullMode=D3D12_CULL_MODE_NONE;
    desc.VS={vs->GetBufferPointer(),vs->GetBufferSize()};desc.PS={ps->GetBufferPointer(),ps->GetBufferSize()};desc.DSVFormat=DXGI_FORMAT_D32_FLOAT;
    desc.DepthStencilState.DepthEnable=TRUE;desc.DepthStencilState.DepthFunc=D3D12_COMPARISON_FUNC_LESS_EQUAL;desc.DepthStencilState.DepthWriteMask=D3D12_DEPTH_WRITE_MASK_ZERO;
    pipeline_=std::make_unique<PipelineState>(device,desc);
}
void SkyPass::Draw(ID3D12GraphicsCommandList* list,FrameContext& frame,const Camera& camera,D3D12_GPU_DESCRIPTOR_HANDLE sky,const RenderSettings& settings){
    if(!settings.skybox||settings.mode!=RenderMode::Final)return;
    struct Constants{DirectX::XMFLOAT4X4 inverseViewProjection;DirectX::XMFLOAT4 camera,parameters;} constants;
    DirectX::XMStoreFloat4x4(&constants.inverseViewProjection,DirectX::XMMatrixInverse(nullptr,camera.View()*camera.Projection()));
    auto p=camera.Position();constants.camera={p.x,p.y,p.z,0};constants.parameters={settings.environmentIntensity,settings.environmentRotation,0,0};
    list->SetGraphicsRootSignature(root_->Get());list->SetPipelineState(pipeline_->Get());list->SetGraphicsRootConstantBufferView(0,frame.constants.Allocate(constants));
    list->SetGraphicsRootDescriptorTable(1,sky);list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);list->DrawInstanced(3,1,0,0);
}
}
