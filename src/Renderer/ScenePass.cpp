#include "Renderer/ScenePass.h"
#include "Renderer/ShaderCompiler.h"
#include <algorithm>
namespace isr {
using namespace DirectX;
ScenePass::ScenePass(ID3D12Device* device,ID3D12GraphicsCommandList* list,DescriptorAllocator& srv,DescriptorAllocator& samplers,const Scene& scene) {
    static_assert(MaterialTextureCount==6); // HLSL t0..t5/s0..s5; lighting begins at t6/s6.
    D3D12_DESCRIPTOR_RANGE ranges[3]={{D3D12_DESCRIPTOR_RANGE_TYPE_SRV,6,0,0,0},{D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER,6,0,0,0},{D3D12_DESCRIPTOR_RANGE_TYPE_SRV,5,6,0,0}};
    D3D12_ROOT_PARAMETER parameters[5]{};parameters[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_CBV;
    parameters[3].ParameterType=D3D12_ROOT_PARAMETER_TYPE_CBV;parameters[3].Descriptor.ShaderRegister=1;
    for(UINT i=0;i<2;++i){parameters[i+1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;parameters[i+1].DescriptorTable={1,&ranges[i]};parameters[i+1].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;}
    parameters[4].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;parameters[4].DescriptorTable={1,&ranges[2]};parameters[4].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_STATIC_SAMPLER_DESC sampler{};sampler.Filter=D3D12_FILTER_MIN_MAG_MIP_LINEAR;sampler.AddressU=sampler.AddressV=sampler.AddressW=D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.ComparisonFunc=D3D12_COMPARISON_FUNC_NONE;sampler.MaxLOD=D3D12_FLOAT32_MAX;sampler.ShaderRegister=6;sampler.ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
    root_=std::make_unique<RootSignature>(device,parameters,D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT,std::span(&sampler,1));
    ShaderCompiler compiler;
#ifdef _DEBUG
    constexpr bool debug=true;
#else
    constexpr bool debug=false;
#endif
    const auto path=ExecutableDirectory()/"shaders/Scene.hlsl";
    auto vs=compiler.Compile(path,L"VSMain",ShaderStage::Vertex,debug),ps=compiler.Compile(path,L"PSMain",ShaderStage::Pixel,debug);
    auto desc=PipelineState::GraphicsDefaults();desc.pRootSignature=root_->Get();desc.RTVFormats[0]=DXGI_FORMAT_R32G32B32A32_FLOAT;
    desc.VS={vs->GetBufferPointer(),vs->GetBufferSize()};desc.PS={ps->GetBufferPointer(),ps->GetBufferSize()};desc.InputLayout=GpuScene::InputLayout();
    desc.DepthStencilState.DepthEnable=TRUE;desc.DepthStencilState.DepthFunc=D3D12_COMPARISON_FUNC_LESS;desc.DSVFormat=DXGI_FORMAT_D32_FLOAT;
    for(unsigned i=0;i<pipelines_.size();++i){
        desc.RasterizerState.FillMode=(i&8)?D3D12_FILL_MODE_WIREFRAME:D3D12_FILL_MODE_SOLID;
        desc.RasterizerState.FrontCounterClockwise=(i&1)!=0;desc.RasterizerState.CullMode=(i&2)?D3D12_CULL_MODE_NONE:D3D12_CULL_MODE_BACK;
        desc.DepthStencilState.DepthWriteMask=(i&4)?D3D12_DEPTH_WRITE_MASK_ZERO:D3D12_DEPTH_WRITE_MASK_ALL;
        auto& blend=desc.BlendState.RenderTarget[0];blend.BlendEnable=(i&4)!=0;blend.SrcBlend=D3D12_BLEND_SRC_ALPHA;blend.DestBlend=D3D12_BLEND_INV_SRC_ALPHA;
        blend.SrcBlendAlpha=D3D12_BLEND_ONE;blend.DestBlendAlpha=D3D12_BLEND_INV_SRC_ALPHA;pipelines_[i]=std::make_unique<PipelineState>(device,desc);
    }
    assets_=std::make_unique<GpuScene>(device,list,srv,samplers,scene);
}
void ScenePass::FinishUpload(){assets_->FinishUpload();}
void ScenePass::Draw(ID3D12GraphicsCommandList* list,FrameContext& frame,const Scene& scene,const RenderSettings& settings,const ShadowFrame& shadow,D3D12_GPU_DESCRIPTOR_HANDLE resources,float maxMip,bool reverseOrder){
    list->SetGraphicsRootSignature(root_->Get());list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    list->SetGraphicsRootDescriptorTable(4,resources);
    FrameConstants lighting{};const auto camera=scene.camera.Position();
    lighting.cameraMode={camera.x,camera.y,camera.z,float(settings.mode)};
    if(scene.lights.size()>MaxLights)throw std::runtime_error("This forward pass supports at most 8 lights");
    lighting.ambientCount={settings.ambient,float(scene.lights.size()),0,0};lighting.nearFar={scene.camera.NearPlane(),scene.camera.FarPlane(),0,0};
    lighting.lightViewProjection=shadow.viewProjection;
    lighting.shadowParams={settings.shadowBias,settings.shadowNormalBias,1.0f/ShadowPass::Resolution,float(settings.shadowPcfRadius)};
    lighting.shadowInfo={float(shadow.lightIndex),float(ShadowPass::Resolution),0,0};
    lighting.environment={settings.environmentIntensity,settings.environmentRotation,maxMip,settings.ibl?1.0f:0.0f};
    for(size_t i=0;i<scene.lights.size();++i){const auto& l=scene.lights[i];
        lighting.lights[i]={{l.position.x,l.position.y,l.position.z,float(l.type)},{l.direction.x,l.direction.y,l.direction.z,l.range},{l.color.x,l.color.y,l.color.z,l.intensity}};}
    list->SetGraphicsRootConstantBufferView(3,frame.constants.Allocate(lighting));
    const auto vp=scene.camera.View()*scene.camera.Projection();std::vector<const Entity*> opaque,transparent;
    for(const auto& e:scene.entities)if(e.renderer&&e.renderer->visible)(scene.materials.at(e.renderer->materialIndex).alphaMode==AlphaMode::Blend?transparent:opaque).push_back(&e);
    if(reverseOrder)std::reverse(opaque.begin(),opaque.end());
    const auto eye=XMLoadFloat3(&camera);auto distance=[&](const Entity* e){return XMVectorGetX(XMVector3LengthSq(e->transform.WorldMatrix().r[3]-eye));};
    std::stable_sort(transparent.begin(),transparent.end(),[&](const Entity* a,const Entity* b){return distance(a)>distance(b);});opaque.insert(opaque.end(),transparent.begin(),transparent.end());
    for(const auto* e:opaque){const auto& r=*e->renderer;const auto& material=scene.materials.at(r.materialIndex);const auto constants=GpuScene::Constants(*e,material,vp);
        const unsigned pipeline=(constants.flags.z<0?1:0)|(material.doubleSided?2:0)|(material.alphaMode==AlphaMode::Blend?4:0)|(settings.mode==RenderMode::Wireframe?8:0);
        list->SetPipelineState(pipelines_[pipeline]->Get());list->SetGraphicsRootConstantBufferView(0,frame.constants.Allocate(constants));
        const auto& binding=assets_->Binding(r.materialIndex);list->SetGraphicsRootDescriptorTable(1,binding.textures);list->SetGraphicsRootDescriptorTable(2,binding.samplers);assets_->Mesh(r.meshIndex).Draw(list);
    }
}
}
