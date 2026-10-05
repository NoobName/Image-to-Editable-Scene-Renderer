#pragma once
#include "Assets/HdrImage.h"
#include "Renderer/DescriptorAllocator.h"
#include "Renderer/RootSignature.h"
#include "Renderer/UploadBuffer.h"
namespace isr {
inline constexpr DXGI_FORMAT EnvironmentFormat=DXGI_FORMAT_R32G32B32A32_FLOAT;
struct EnvironmentMaps { ComPtr<ID3D12Resource> sky,irradiance,prefilter; };
class EnvironmentBaker {
public:
    static constexpr UINT SkySize=256,SkyMips=9,PrefilterSize=128,PrefilterMips=8,IrradianceSize=32,LutSize=128;
    explicit EnvironmentBaker(DeviceContext&);
    EnvironmentMaps Bake(HdrImage);
    ID3D12Resource* BrdfLut() const{return lut_.Get();}
    static void CubeSrv(ID3D12Device*,ID3D12Resource*,UINT mips,D3D12_CPU_DESCRIPTOR_HANDLE);
    static void ImageSrv(ID3D12Device*,ID3D12Resource*,UINT mips,D3D12_CPU_DESCRIPTOR_HANDLE);
private:
    ComPtr<ID3D12Resource> MakeTexture(UINT width,UINT height,UINT layers,UINT mips,D3D12_RESOURCE_FLAGS,D3D12_RESOURCE_STATES,const wchar_t*);
    void Begin();void Submit();
    void Dispatch(ID3D12Resource*,UINT mip,UINT size,UINT faces,UINT mode,float roughness,float panoramaLod,UINT descriptor);
    DeviceContext& context_;
    DescriptorAllocator heap_;
    std::unique_ptr<RootSignature> root_;
    ComPtr<ID3D12PipelineState> pipeline_;
    ComPtr<ID3D12CommandAllocator> allocator_;
    ComPtr<ID3D12GraphicsCommandList> list_;
    ComPtr<ID3D12Resource> lut_;
};
}
