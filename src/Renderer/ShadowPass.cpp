#include "Renderer/ShadowPass.h"
#include "Renderer/ShaderCompiler.h"
namespace isr {
using namespace DirectX;
ShadowPass::ShadowPass(ID3D12Device* device,DescriptorAllocator& dsv,D3D12_CPU_DESCRIPTOR_HANDLE srv,const Scene& scene){
    for(const auto& mesh:scene.meshes)meshBounds_.push_back(MeshBounds(mesh));
    dsv_=dsv.Allocate();const D3D12_CLEAR_VALUE clear{DXGI_FORMAT_D32_FLOAT,{.DepthStencil={1,0}}};
    depth_=std::make_unique<Texture>(device,Resolution,Resolution,DXGI_FORMAT_R32_TYPELESS,D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,D3D12_RESOURCE_STATE_DEPTH_WRITE,&clear);
    D3D12_DEPTH_STENCIL_VIEW_DESC depthView{};depthView.Format=DXGI_FORMAT_D32_FLOAT;depthView.ViewDimension=D3D12_DSV_DIMENSION_TEXTURE2D;
    device->CreateDepthStencilView(depth_->Resource(),&depthView,dsv_.cpu);
    D3D12_SHADER_RESOURCE_VIEW_DESC view{};view.Format=DXGI_FORMAT_R32_FLOAT;view.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;view.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;view.Texture2D.MipLevels=1;
    device->CreateShaderResourceView(depth_->Resource(),&view,srv);
    D3D12_DESCRIPTOR_RANGE ranges[2]={{D3D12_DESCRIPTOR_RANGE_TYPE_SRV,6,0,0,0},{D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER,6,0,0,0}};
    D3D12_ROOT_PARAMETER params[3]{};params[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_CBV;
    for(UINT i=0;i<2;++i){params[i+1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;params[i+1].DescriptorTable={1,&ranges[i]};params[i+1].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;}
    root_=std::make_unique<RootSignature>(device,params,D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
    ShaderCompiler compiler;
#ifdef _DEBUG
    constexpr bool debug=true;
#else
    constexpr bool debug=false;
#endif
    auto path=ExecutableDirectory()/"shaders/Shadow.hlsl";auto vs=compiler.Compile(path,L"VSMain",ShaderStage::Vertex,debug),ps=compiler.Compile(path,L"PSMain",ShaderStage::Pixel,debug);
    auto desc=PipelineState::GraphicsDefaults();desc.pRootSignature=root_->Get();desc.InputLayout=GpuScene::InputLayout();desc.VS={vs->GetBufferPointer(),vs->GetBufferSize()};desc.PS={ps->GetBufferPointer(),ps->GetBufferSize()};
    desc.NumRenderTargets=0;desc.RTVFormats[0]=DXGI_FORMAT_UNKNOWN;desc.DSVFormat=DXGI_FORMAT_D32_FLOAT;
    desc.DepthStencilState.DepthEnable=TRUE;desc.DepthStencilState.DepthWriteMask=D3D12_DEPTH_WRITE_MASK_ALL;desc.DepthStencilState.DepthFunc=D3D12_COMPARISON_FUNC_LESS;
    for(unsigned i=0;i<4;++i){desc.RasterizerState.FrontCounterClockwise=(i&1)!=0;desc.RasterizerState.CullMode=(i&2)?D3D12_CULL_MODE_NONE:D3D12_CULL_MODE_BACK;pipelines_[i]=std::make_unique<PipelineState>(device,desc);}
}
ShadowFrame ShadowPass::Draw(ID3D12GraphicsCommandList* list,FrameContext& frame,const Scene& scene,const GpuScene& gpu,const RenderSettings& settings,const XMFLOAT3* fixedDirection){
    ShadowFrame result;XMStoreFloat4x4(&result.viewProjection,XMMatrixIdentity());
    if(fixedDirection){result.lightIndex=0;XMStoreFloat4x4(&result.viewProjection,FitDirectionalShadow(WorldBounds(scene,meshBounds_),*fixedDirection,Resolution));}
    else if(settings.shadows)for(size_t i=0;i<scene.lights.size();++i){const auto& light=scene.lights[i];
        if(light.type==LightType::Directional&&light.intensity>0&&XMVectorGetX(XMVector3LengthSq(XMLoadFloat3(&light.direction)))>1e-10f){
            result.lightIndex=static_cast<int>(i);XMStoreFloat4x4(&result.viewProjection,FitDirectionalShadow(WorldBounds(scene,meshBounds_),light.direction,Resolution));break;}}
    depth_->Transition(list,D3D12_RESOURCE_STATE_DEPTH_WRITE);list->ClearDepthStencilView(dsv_.cpu,D3D12_CLEAR_FLAG_DEPTH,1,0,0,nullptr);
    if(result.lightIndex>=0){
        const D3D12_VIEWPORT viewport{0,0,float(Resolution),float(Resolution),0,1};const D3D12_RECT scissor{0,0,Resolution,Resolution};
        list->RSSetViewports(1,&viewport);list->RSSetScissorRects(1,&scissor);list->OMSetRenderTargets(0,nullptr,FALSE,&dsv_.cpu);
        list->SetGraphicsRootSignature(root_->Get());list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        for(const auto& e:scene.entities)if(e.renderer&&e.renderer->visible){const auto& r=*e.renderer;const auto& m=scene.materials.at(r.materialIndex);
            if(m.alphaMode==AlphaMode::Blend)continue; // No physically meaningful opaque depth for blended surfaces.
            const auto c=GpuScene::Constants(e,m,XMLoadFloat4x4(&result.viewProjection));unsigned p=(c.flags.z<0?1:0)|(m.doubleSided?2:0);
            list->SetPipelineState(pipelines_[p]->Get());list->SetGraphicsRootConstantBufferView(0,frame.constants.Allocate(c));
            const auto& binding=gpu.Binding(r.materialIndex);list->SetGraphicsRootDescriptorTable(1,binding.textures);list->SetGraphicsRootDescriptorTable(2,binding.samplers);gpu.Mesh(r.meshIndex).Draw(list);
        }
    }
    depth_->Transition(list,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);return result;
}
}
