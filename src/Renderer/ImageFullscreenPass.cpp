#include "Renderer/ImageFullscreenPass.h"
#include "Renderer/ShaderCompiler.h"
namespace isr {
ImageFullscreenPass::ImageFullscreenPass(ID3D12Device* device,const std::filesystem::path& path,const wchar_t* entry,DXGI_FORMAT format,UINT inputs,UINT constants)
    :inputs_(inputs),constants_(constants){
    if(!inputs||inputs>16||!constants||constants>40||inputs+constants>64)throw std::invalid_argument("Image root signature budget exceeded");
    std::vector<D3D12_DESCRIPTOR_RANGE> ranges(inputs);std::vector<D3D12_ROOT_PARAMETER> params(inputs+1);
    for(UINT i=0;i<inputs;++i){ranges[i]={D3D12_DESCRIPTOR_RANGE_TYPE_SRV,1,i,0,0};params[i].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        params[i].DescriptorTable={1,&ranges[i]};params[i].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;}
    params.back().ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;params.back().Constants={0,0,constants};params.back().ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
    root_=std::make_unique<RootSignature>(device,params,D3D12_ROOT_SIGNATURE_FLAG_NONE);
#ifdef _DEBUG
    constexpr bool debug=true;
#else
    constexpr bool debug=false;
#endif
    ShaderCompiler compiler;auto vs=compiler.Compile(path,L"VSMain",ShaderStage::Vertex,debug),ps=compiler.Compile(path,entry,ShaderStage::Pixel,debug);
    auto desc=PipelineState::GraphicsDefaults();desc.pRootSignature=root_->Get();desc.RasterizerState.CullMode=D3D12_CULL_MODE_NONE;desc.RTVFormats[0]=format;
    desc.VS={vs->GetBufferPointer(),vs->GetBufferSize()};desc.PS={ps->GetBufferPointer(),ps->GetBufferSize()};pipeline_=std::make_unique<PipelineState>(device,desc);
}
void ImageFullscreenPass::Draw(ID3D12GraphicsCommandList* list,D3D12_CPU_DESCRIPTOR_HANDLE out,UINT w,UINT h,std::span<const D3D12_GPU_DESCRIPTOR_HANDLE> views,const void* data)const{
    if(views.size()!=inputs_)throw std::invalid_argument("Image SRV count mismatch");
    const D3D12_VIEWPORT vp{0,0,float(w),float(h),0,1};const D3D12_RECT scissor{0,0,LONG(w),LONG(h)};
    list->RSSetViewports(1,&vp);list->RSSetScissorRects(1,&scissor);list->OMSetRenderTargets(1,&out,FALSE,nullptr);
    list->SetGraphicsRootSignature(root_->Get());list->SetPipelineState(pipeline_->Get());
    for(UINT i=0;i<inputs_;++i)list->SetGraphicsRootDescriptorTable(i,views[i]);
    list->SetGraphicsRoot32BitConstants(inputs_,constants_,data,0);list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);list->DrawInstanced(3,1,0,0);
}
}
