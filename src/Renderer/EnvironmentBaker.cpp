#include "Renderer/EnvironmentBaker.h"
#include "Renderer/ShaderCompiler.h"
#include "Core/Log.h"
#include <algorithm>
#include <cmath>
namespace isr {
namespace {
void Transition(ID3D12GraphicsCommandList* list,ID3D12Resource* resource,D3D12_RESOURCE_STATES before,D3D12_RESOURCE_STATES after){
    D3D12_RESOURCE_BARRIER barrier{};barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;barrier.Transition={resource,D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,before,after};list->ResourceBarrier(1,&barrier);
}
}
ComPtr<ID3D12Resource> EnvironmentBaker::MakeTexture(UINT w,UINT h,UINT layers,UINT mips,D3D12_RESOURCE_FLAGS flags,D3D12_RESOURCE_STATES state,const wchar_t* name){
    D3D12_RESOURCE_DESC desc{};desc.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;desc.Width=w;desc.Height=h;desc.DepthOrArraySize=static_cast<UINT16>(layers);desc.MipLevels=static_cast<UINT16>(mips);desc.Format=EnvironmentFormat;desc.SampleDesc.Count=1;desc.Flags=flags;
    D3D12_HEAP_PROPERTIES props{};props.Type=D3D12_HEAP_TYPE_DEFAULT;ComPtr<ID3D12Resource> resource;
    Check(context_.Device()->CreateCommittedResource(&props,D3D12_HEAP_FLAG_NONE,&desc,state,nullptr,IID_PPV_ARGS(&resource)));Check(resource->SetName(name));return resource;
}
void EnvironmentBaker::CubeSrv(ID3D12Device* device,ID3D12Resource* resource,UINT mips,D3D12_CPU_DESCRIPTOR_HANDLE handle){
    D3D12_SHADER_RESOURCE_VIEW_DESC d{};d.Format=EnvironmentFormat;d.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;d.ViewDimension=D3D12_SRV_DIMENSION_TEXTURECUBE;d.TextureCube.MipLevels=mips;device->CreateShaderResourceView(resource,&d,handle);
}
void EnvironmentBaker::ImageSrv(ID3D12Device* device,ID3D12Resource* resource,UINT mips,D3D12_CPU_DESCRIPTOR_HANDLE handle){
    D3D12_SHADER_RESOURCE_VIEW_DESC d{};d.Format=EnvironmentFormat;d.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;d.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;d.Texture2D.MipLevels=mips;device->CreateShaderResourceView(resource,&d,handle);
}
EnvironmentBaker::EnvironmentBaker(DeviceContext& context):context_(context),heap_(context.Device(),D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,32,true){
    heap_.Allocate(32);D3D12_FEATURE_DATA_FORMAT_SUPPORT support{EnvironmentFormat};Check(context.Device()->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT,&support,sizeof(support)));
    if(!(support.Support2&D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE))throw std::runtime_error("RGBA32F UAV stores required for IBL baking");
    D3D12_DESCRIPTOR_RANGE ranges[2]={{D3D12_DESCRIPTOR_RANGE_TYPE_SRV,2,0,0,0},{D3D12_DESCRIPTOR_RANGE_TYPE_UAV,1,0,0,0}};
    D3D12_ROOT_PARAMETER params[3]{};for(UINT i=0;i<2;++i){params[i].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;params[i].DescriptorTable={1,&ranges[i]};}
    params[2].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;params[2].Constants={2,0,8};
    D3D12_STATIC_SAMPLER_DESC samplers[2]{};
    for(UINT i=0;i<2;++i){auto& s=samplers[i];s.Filter=D3D12_FILTER_MIN_MAG_MIP_LINEAR;s.AddressU=s.AddressV=s.AddressW=D3D12_TEXTURE_ADDRESS_MODE_CLAMP;s.ComparisonFunc=D3D12_COMPARISON_FUNC_NONE;s.MaxLOD=D3D12_FLOAT32_MAX;s.ShaderRegister=i;}
    samplers[0].AddressU=D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    root_=std::make_unique<RootSignature>(context.Device(),params,D3D12_ROOT_SIGNATURE_FLAG_NONE,samplers);
    ShaderCompiler compiler;
#ifdef _DEBUG
    constexpr bool debug=true;
#else
    constexpr bool debug=false;
#endif
    auto cs=compiler.Compile(ExecutableDirectory()/"shaders/EnvironmentBake.hlsl",L"CSMain",ShaderStage::Compute,debug);
    D3D12_COMPUTE_PIPELINE_STATE_DESC desc{};desc.pRootSignature=root_->Get();desc.CS={cs->GetBufferPointer(),cs->GetBufferSize()};Check(context.Device()->CreateComputePipelineState(&desc,IID_PPV_ARGS(&pipeline_)));
    Check(context.Device()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator_)));
    Check(context.Device()->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator_.Get(),nullptr,IID_PPV_ARGS(&list_)));Check(list_->Close());
    ImageSrv(context.Device(),nullptr,1,heap_.Cpu(4));CubeSrv(context.Device(),nullptr,1,heap_.Cpu(5));
    lut_=MakeTexture(LutSize,LutSize,1,1,D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,L"Split sum BRDF LUT");
    Begin();list_->SetComputeRootDescriptorTable(0,heap_.Gpu(4));Dispatch(lut_.Get(),0,LutSize,1,3,0,0,24);
    Transition(list_.Get(),lut_.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);Submit();
}
void EnvironmentBaker::Begin(){
    // Each previous bake has completed before allocator/descriptors are reused.
    Check(allocator_->Reset());Check(list_->Reset(allocator_.Get(),pipeline_.Get()));ID3D12DescriptorHeap* heaps[]={heap_.Heap()};list_->SetDescriptorHeaps(1,heaps);list_->SetComputeRootSignature(root_->Get());
}
void EnvironmentBaker::Submit(){Check(list_->Close());ID3D12CommandList* lists[]={list_.Get()};context_.Queue()->ExecuteCommandLists(1,lists);context_.Flush();context_.CheckMessages();}
void EnvironmentBaker::Dispatch(ID3D12Resource* output,UINT mip,UINT size,UINT faces,UINT mode,float roughness,float inputLod,UINT descriptor){
    D3D12_UNORDERED_ACCESS_VIEW_DESC view{};view.Format=EnvironmentFormat;view.ViewDimension=D3D12_UAV_DIMENSION_TEXTURE2DARRAY;view.Texture2DArray.MipSlice=mip;view.Texture2DArray.ArraySize=faces;
    context_.Device()->CreateUnorderedAccessView(output,nullptr,&view,heap_.Cpu(descriptor));list_->SetComputeRootDescriptorTable(1,heap_.Gpu(descriptor));
    struct Parameters {UINT size,mode;float roughness;UINT samples;float inputLod,cubeSize,pad[2];} constants{size,mode,roughness,256,inputLod,float(SkySize),{0,0}};
    list_->SetComputeRoot32BitConstants(2,8,&constants,0);list_->Dispatch((size+7)/8,(size+7)/8,faces);
}
EnvironmentMaps EnvironmentBaker::Bake(HdrImage image){
    auto mips=HdrMipChain(std::move(image));auto source=MakeTexture(mips[0].width,mips[0].height,1,static_cast<UINT>(mips.size()),D3D12_RESOURCE_FLAG_NONE,D3D12_RESOURCE_STATE_COPY_DEST,L"HDR panorama");
    auto desc=source->GetDesc();std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> footprints(mips.size());UINT64 bytes=0;
    context_.Device()->GetCopyableFootprints(&desc,0,static_cast<UINT>(mips.size()),0,footprints.data(),nullptr,nullptr,&bytes);UploadBuffer upload(context_.Device(),static_cast<size_t>(bytes));
    EnvironmentMaps maps;maps.sky=MakeTexture(SkySize,SkySize,6,SkyMips,D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,L"Environment sky cubemap");
    maps.irradiance=MakeTexture(IrradianceSize,IrradianceSize,6,1,D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,L"Diffuse irradiance");
    maps.prefilter=MakeTexture(PrefilterSize,PrefilterSize,6,PrefilterMips,D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,L"GGX prefilter");
    ImageSrv(context_.Device(),source.Get(),static_cast<UINT>(mips.size()),heap_.Cpu(0));CubeSrv(context_.Device(),nullptr,1,heap_.Cpu(1));
    ImageSrv(context_.Device(),source.Get(),static_cast<UINT>(mips.size()),heap_.Cpu(2));CubeSrv(context_.Device(),maps.sky.Get(),SkyMips,heap_.Cpu(3));
    Begin();
    for(UINT mip=0;mip<mips.size();++mip){for(UINT y=0;y<mips[mip].height;++y)upload.Write(static_cast<size_t>(footprints[mip].Offset)+size_t(y)*footprints[mip].Footprint.RowPitch,mips[mip].pixels.data()+size_t(y)*mips[mip].width,size_t(mips[mip].width)*sizeof(DirectX::XMFLOAT4));
        D3D12_TEXTURE_COPY_LOCATION dst{};dst.pResource=source.Get();dst.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;dst.SubresourceIndex=mip;
        D3D12_TEXTURE_COPY_LOCATION src{};src.pResource=upload.Resource();src.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;src.PlacedFootprint=footprints[mip];list_->CopyTextureRegion(&dst,0,0,0,&src,nullptr);}
    Transition(list_.Get(),source.Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    list_->SetComputeRootDescriptorTable(0,heap_.Gpu(0));
    for(UINT mip=0;mip<SkyMips;++mip)Dispatch(maps.sky.Get(),mip,std::max(1u,SkySize>>mip),6,0,0,std::max(0.0f,std::log2(float(mips[0].width)/(SkySize*4)))+mip,6+mip);
    Transition(list_.Get(),maps.sky.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    list_->SetComputeRootDescriptorTable(0,heap_.Gpu(2));Dispatch(maps.irradiance.Get(),0,IrradianceSize,6,1,0,0,23);
    for(UINT mip=0;mip<PrefilterMips;++mip)Dispatch(maps.prefilter.Get(),mip,std::max(1u,PrefilterSize>>mip),6,2,float(mip)/(PrefilterMips-1),0,15+mip);
    Transition(list_.Get(),maps.irradiance.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    Transition(list_.Get(),maps.prefilter.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    Transition(list_.Get(),maps.sky.Get(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);Submit();return maps;
}
}
