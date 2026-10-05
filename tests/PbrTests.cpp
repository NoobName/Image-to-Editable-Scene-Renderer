#include "Renderer/DeviceContext.h"
#include "Renderer/RootSignature.h"
#include "Renderer/ShaderCompiler.h"
#include <DirectXMath.h>
#include <cmath>
#include <algorithm>
#include <iostream>
using namespace isr;
int main(){try{
    DeviceContext context(true);ComPtr<ID3D12CommandAllocator> allocator;ComPtr<ID3D12GraphicsCommandList> list;
    Check(context.Device()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator)));
    Check(context.Device()->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&list)));
    D3D12_ROOT_PARAMETER parameter{};parameter.ParameterType=D3D12_ROOT_PARAMETER_TYPE_UAV;
    RootSignature root(context.Device(),std::span(&parameter,1),D3D12_ROOT_SIGNATURE_FLAG_NONE);
    ShaderCompiler compiler;
#ifdef _DEBUG
    constexpr bool debug=true;
#else
    constexpr bool debug=false;
#endif
    auto shader=compiler.Compile(ExecutableDirectory()/"shaders/PbrValidation.hlsl",L"CSMain",ShaderStage::Compute,debug);
    D3D12_COMPUTE_PIPELINE_STATE_DESC pso{};pso.pRootSignature=root.Get();pso.CS={shader->GetBufferPointer(),shader->GetBufferSize()};
    ComPtr<ID3D12PipelineState> pipeline;Check(context.Device()->CreateComputePipelineState(&pso,IID_PPV_ARGS(&pipeline)));
    constexpr size_t bytes=512*16;D3D12_RESOURCE_DESC desc{};desc.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;desc.Width=bytes;desc.Height=1;desc.DepthOrArraySize=1;desc.MipLevels=1;desc.SampleDesc.Count=1;desc.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;desc.Flags=D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_DEFAULT;ComPtr<ID3D12Resource> output,readback;
    Check(context.Device()->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,nullptr,IID_PPV_ARGS(&output)));
    heap.Type=D3D12_HEAP_TYPE_READBACK;desc.Flags=D3D12_RESOURCE_FLAG_NONE;
    Check(context.Device()->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&readback)));
    list->SetComputeRootSignature(root.Get());list->SetPipelineState(pipeline.Get());list->SetComputeRootUnorderedAccessView(0,output->GetGPUVirtualAddress());list->Dispatch(8,1,1);
    D3D12_RESOURCE_BARRIER barrier{};barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;barrier.Transition={output.Get(),D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COPY_SOURCE};list->ResourceBarrier(1,&barrier);
    list->CopyBufferRegion(readback.Get(),0,output.Get(),0,bytes);Check(list->Close());ID3D12CommandList* lists[]={list.Get()};context.Queue()->ExecuteCommandLists(1,lists);context.Flush();
    void* mapped{};D3D12_RANGE range{0,bytes};Check(readback->Map(0,&range,&mapped));
    const auto* values=static_cast<const DirectX::XMFLOAT4*>(mapped);bool ok=true;
    constexpr float pi=DirectX::XM_PI;
    const float expected[]={0.49f/pi,0.125f/pi,1,0.04f,0};
    for(unsigned i=0;i<5;++i)ok&=std::abs(values[i].x-expected[i])<1e-5f;
    for(unsigned i=0;i<256;++i)ok&=std::isfinite(values[i].x+values[i].y+values[i].z)&&values[i].x>=0&&values[i].y>=0&&values[i].z>=0;
    for(unsigned i=0;i<72;++i){
        constexpr double radiance[]={0,0.18,1,16},gamma[]={0.5,1,2};
        double value=radiance[i%4]*std::exp2(double((i/4)%2)*2);
        if(i/24==1)value=value/(1+value);
        if(i/24==2)value=(value*(2.51*value+0.03))/(value*(2.43*value+0.59)+0.14);
        value=std::pow(std::clamp(value,0.0,1.0),1/gamma[(i/8)%3]);
        const double encoded=value<=0.0031308?value*12.92:1.055*std::pow(value,1/2.4)-0.055;
        ok&=std::abs(values[256+i].x-encoded)<2e-5&&std::abs(values[256+i].y-encoded)<2e-5&&std::abs(values[256+i].z-encoded)<2e-5;
    }
    range={0,0};readback->Unmap(0,&range);context.CheckMessages();
    if(!ok)throw std::runtime_error("GPU BRDF differs from analytic limits or produced nonfinite/negative radiance");
    std::cout<<"PASS: 256 GPU BRDF samples and 72 color-management samples (3 tone maps, exposure, gamma, exact sRGB)\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
