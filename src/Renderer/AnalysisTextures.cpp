#include "Renderer/AnalysisTextures.h"
#include "Renderer/ShaderCompiler.h"
namespace isr {
AnalysisTextures::AnalysisTextures(ID3D12Device* device,ID3D12GraphicsCommandList* list,DescriptorAllocator& heap,std::shared_ptr<const AnalysisMaps> data):data_(std::move(data)){
    for(size_t i=0;i<maps_.size();++i)if(data_&&data_->maps[i].image)maps_[i]=std::make_unique<NumericTexture>(device,list,heap,*data_->maps[i].image);
    nullFloat_=heap.Allocate();nullUint_=heap.Allocate();
    D3D12_SHADER_RESOURCE_VIEW_DESC srv{};srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;srv.Texture2D.MipLevels=1;
    srv.Format=DXGI_FORMAT_R32G32B32A32_FLOAT;device->CreateShaderResourceView(nullptr,&srv,nullFloat_.cpu);
    srv.Format=DXGI_FORMAT_R32_UINT;device->CreateShaderResourceView(nullptr,&srv,nullUint_.cpu);
    std::array<D3D12_DESCRIPTOR_RANGE,5> ranges{};std::array<D3D12_ROOT_PARAMETER,6> params{};
    for(UINT i=0;i<5;++i){ranges[i]={D3D12_DESCRIPTOR_RANGE_TYPE_SRV,1,i,0,0};params[i].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        params[i].DescriptorTable={1,&ranges[i]};params[i].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;}
    params[5].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;params[5].Constants={0,0,16};params[5].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
    root_=std::make_unique<RootSignature>(device,params,D3D12_ROOT_SIGNATURE_FLAG_NONE);
#ifdef _DEBUG
    constexpr bool debug=true;
#else
    constexpr bool debug=false;
#endif
    ShaderCompiler compiler;const auto path=ExecutableDirectory()/"shaders/AnalysisImage.hlsl";
    auto vs=compiler.Compile(path,L"VSMain",ShaderStage::Vertex,debug);
    auto make=[&](const wchar_t* entry,DXGI_FORMAT format){auto ps=compiler.Compile(path,entry,ShaderStage::Pixel,debug);auto desc=PipelineState::GraphicsDefaults();
        desc.pRootSignature=root_->Get();desc.RasterizerState.CullMode=D3D12_CULL_MODE_NONE;desc.RTVFormats[0]=format;
        desc.VS={vs->GetBufferPointer(),vs->GetBufferSize()};desc.PS={ps->GetBufferPointer(),ps->GetBufferSize()};return std::make_unique<PipelineState>(device,desc);};
    pipeline_=make(L"PSMain",DXGI_FORMAT_R8G8B8A8_UNORM);rawPipeline_=make(L"PSRaw",DXGI_FORMAT_R32G32B32A32_FLOAT);
}
void AnalysisTextures::FinishUpload(){for(auto& map:maps_)if(map)map->FinishUpload();}
void AnalysisTextures::Draw(ID3D12GraphicsCommandList* list,D3D12_CPU_DESCRIPTOR_HANDLE output,uint32_t vw,uint32_t vh,uint32_t sw,uint32_t sh,size_t selected,bool raw)const{
    if(selected>=maps_.size())throw std::out_of_range("Unknown analysis map");const bool available=maps_[selected]!=nullptr;
    const auto rect=FitSourceImage(sw,sh,vw,vh);const auto* m=data_?&data_->maps[selected].metadata:nullptr;
    const bool integer=selected==4||selected==5;
    auto view=[&](size_t i,bool uintType){return maps_[i]?maps_[i]->View():uintType?nullUint_.gpu:nullFloat_.gpu;};
    struct Constants {float rect[4];uint32_t width,height,selected,available;float low,high,edge;uint32_t mask;uint32_t hasDepth,hasRegions,pad0,pad1;};
    const bool geometry=selected<=3||selected==9;
    const Constants constants{{rect.x,rect.y,rect.width,rect.height},data_?data_->metadata["analysisSize"][0].get<uint32_t>():1,
        data_?data_->metadata["analysisSize"][1].get<uint32_t>():1,uint32_t(selected),uint32_t(available),
        available?(*m)["range"][0].get<float>():0,available?(*m)["range"][1].get<float>():1,data_?data_->metadata["sampling"]["relativeDepthThreshold"].get<float>():.15f,
        geometry?1u:selected==11?2u:0u,uint32_t(maps_[0]!=nullptr),uint32_t(maps_[5]!=nullptr),0,0};
    const D3D12_VIEWPORT vp{0,0,float(vw),float(vh),0,1};const D3D12_RECT scissor{0,0,LONG(vw),LONG(vh)};
    list->RSSetViewports(1,&vp);list->RSSetScissorRects(1,&scissor);list->OMSetRenderTargets(1,&output,FALSE,nullptr);
    list->SetGraphicsRootSignature(root_->Get());list->SetPipelineState(raw?rawPipeline_->Get():pipeline_->Get());
    list->SetGraphicsRootDescriptorTable(0,!integer&&available?view(selected,false):nullFloat_.gpu);
    list->SetGraphicsRootDescriptorTable(1,integer&&available?view(selected,true):nullUint_.gpu);
    list->SetGraphicsRootDescriptorTable(2,view(4,true));list->SetGraphicsRootDescriptorTable(3,view(0,false));list->SetGraphicsRootDescriptorTable(4,view(5,true));
    list->SetGraphicsRoot32BitConstants(5,16,&constants,0);list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);list->DrawInstanced(3,1,0,0);
}
}
