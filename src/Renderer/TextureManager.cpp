#include "Renderer/TextureManager.h"
#include "Assets/TextureProcessing.h"
#include "Core/Log.h"
namespace isr {
TextureManager::GpuImage& TextureManager::Upload(ID3D12Device* device,ID3D12GraphicsCommandList* list,std::shared_ptr<const ImageData> image,bool srgb) {
    const auto key=std::make_pair(image.get(),srgb);auto found=resources_.find(key);if(found!=resources_.end())return found->second;
    auto mips=BuildMipChain(*image,srgb);GpuImage result;result.levels=static_cast<UINT>(mips.size());result.srgb=srgb;
    D3D12_RESOURCE_DESC desc{};desc.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;desc.Width=image->width;desc.Height=image->height;
    desc.DepthOrArraySize=1;desc.MipLevels=static_cast<UINT16>(mips.size());desc.Format=DXGI_FORMAT_R8G8B8A8_TYPELESS;desc.SampleDesc.Count=1;
    D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_DEFAULT;
    Check(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&result.resource)));
    std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> layouts(mips.size());UINT64 size=0;
    device->GetCopyableFootprints(&desc,0,result.levels,0,layouts.data(),nullptr,nullptr,&size);
    result.upload=std::make_unique<UploadBuffer>(device,static_cast<size_t>(size));
    for(UINT i=0;i<result.levels;++i) {
        for(UINT y=0;y<mips[i].height;++y)result.upload->Write(size_t(layouts[i].Offset)+size_t(y)*layouts[i].Footprint.RowPitch,
            mips[i].rgba.data()+size_t(y)*mips[i].width*4,size_t(mips[i].width)*4);
        D3D12_TEXTURE_COPY_LOCATION dst{};dst.pResource=result.resource.Get();dst.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;dst.SubresourceIndex=i;
        D3D12_TEXTURE_COPY_LOCATION src{};src.pResource=result.upload->Resource();src.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;src.PlacedFootprint=layouts[i];
        list->CopyTextureRegion(&dst,0,0,0,&src,nullptr);
    }
    D3D12_RESOURCE_BARRIER barrier{};barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition={result.resource.Get(),D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE};
    list->ResourceBarrier(1,&barrier);retainedImages_.push_back(std::move(image));
    return resources_.emplace(key,std::move(result)).first->second;
}
static D3D12_SAMPLER_DESC Sampler(const TextureAsset& texture) {
    auto address=[](int mode){return mode==33071?D3D12_TEXTURE_ADDRESS_MODE_CLAMP:mode==33648?D3D12_TEXTURE_ADDRESS_MODE_MIRROR:D3D12_TEXTURE_ADDRESS_MODE_WRAP;};
    const bool minLinear=texture.minFilter==9729||texture.minFilter==9985||texture.minFilter==9987;
    const bool mipLinear=texture.minFilter==9986||texture.minFilter==9987;
    D3D12_SAMPLER_DESC desc{};
    desc.Filter=D3D12_ENCODE_BASIC_FILTER(minLinear?D3D12_FILTER_TYPE_LINEAR:D3D12_FILTER_TYPE_POINT,
        texture.magFilter==9728?D3D12_FILTER_TYPE_POINT:D3D12_FILTER_TYPE_LINEAR,mipLinear?D3D12_FILTER_TYPE_LINEAR:D3D12_FILTER_TYPE_POINT,D3D12_FILTER_REDUCTION_TYPE_STANDARD);
    desc.AddressU=address(texture.wrapS);desc.AddressV=address(texture.wrapT);desc.AddressW=D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    desc.ComparisonFunc=D3D12_COMPARISON_FUNC_NONE;desc.MaxAnisotropy=1;
    desc.MaxLOD=texture.minFilter==9728||texture.minFilter==9729?0:D3D12_FLOAT32_MAX;return desc;
}
TextureManager::TextureManager(ID3D12Device* device,ID3D12GraphicsCommandList* list,DescriptorAllocator& srv,DescriptorAllocator& samplers,const Scene& scene) {
    auto white=std::make_shared<ImageData>();white->name="White fallback";white->width=white->height=1;white->rgba={255,255,255,255};
    auto normal=std::make_shared<ImageData>(*white);normal->name="Normal fallback";normal->rgba={128,128,255,255};
    std::map<std::array<int,MaterialTextureCount*4>,DescriptorAllocation> samplerTables;
    for(const auto& material:scene.materials) {
        std::array<int,MaterialTextureCount*4> key{};
        for(size_t i=0;i<MaterialTextureCount;++i){TextureAsset asset;if(material.textures[i].texture)asset=scene.textures.at(*material.textures[i].texture);
            key[i*4]=asset.minFilter;key[i*4+1]=asset.magFilter;key[i*4+2]=asset.wrapS;key[i*4+3]=asset.wrapT;}
        const auto found=samplerTables.find(key);const bool create=found==samplerTables.end();
        const auto states=create?samplers.Allocate(MaterialTextureCount):found->second;
        if(create)samplerTables.emplace(key,states);
        const auto textures=srv.Allocate(MaterialTextureCount);
        bindings_.push_back({textures.gpu,states.gpu});
        for(size_t i=0;i<MaterialTextureCount;++i) {
            const auto role=static_cast<TextureRole>(i);TextureAsset asset;
            if(material.textures[i].texture)asset=scene.textures.at(*material.textures[i].texture);
            else asset.image=role==TextureRole::Normal?normal:white;
            auto& gpu=Upload(device,list,asset.image,IsSrgb(role));
            D3D12_SHADER_RESOURCE_VIEW_DESC view{};view.Format=gpu.srgb?DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:DXGI_FORMAT_R8G8B8A8_UNORM;
            view.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;view.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;view.Texture2D.MipLevels=gpu.levels;
            device->CreateShaderResourceView(gpu.resource.Get(),&view,srv.Cpu(textures.index+static_cast<UINT>(i)));
            if(create){const auto sampler=Sampler(asset);device->CreateSampler(&sampler,samplers.Cpu(states.index+static_cast<UINT>(i)));}
        }
    }
    Log("TextureManager: unique GPU images="+std::to_string(resources_.size())+" (image + color-space cache, including fallbacks)");
}
void TextureManager::FinishUpload(){for(auto& [key,image]:resources_)image.upload.reset();}
}
