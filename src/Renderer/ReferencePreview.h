#pragma once
#include "Renderer/SourceImagePass.h"
namespace isr {
// Two read-only preview textures; numeric fit evidence remains in the analysis document.
struct ReferencePreview {
    std::shared_ptr<const ReferenceAnalysis> analysis;
    DescriptorAllocator resources;
    std::array<std::unique_ptr<SourceImagePass>,2> images;
    ReferencePreview(ID3D12Device* device,ID3D12GraphicsCommandList* list,std::shared_ptr<const ReferenceAnalysis> data)
        :analysis(std::move(data)),resources(device,D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,2,true){
        for(size_t i=0;i<2;++i)images[i]=std::make_unique<SourceImagePass>(device,list,resources,*analysis->previews[i]);
    }
};
}
