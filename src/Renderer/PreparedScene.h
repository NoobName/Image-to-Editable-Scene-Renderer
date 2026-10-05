#pragma once
#include "Renderer/ScenePass.h"
#include "Renderer/SourceImagePass.h"
#include "Renderer/AnalysisTextures.h"
#include "Renderer/LightingPreview.h"
namespace isr {
// Owns a new scene's descriptors and GPU resources; does not touch active frame heaps.
struct PreparedScene {
    DescriptorAllocator resources,samplers,dsv;
    DescriptorAllocation lighting;
    std::unique_ptr<ScenePass> scene;
    std::unique_ptr<ShadowPass> shadow;
    std::unique_ptr<SourceImagePass> sourceImage;
    std::unique_ptr<AnalysisTextures> analysis;
    std::unique_ptr<LightingPreview> lightingPreview;
    std::shared_ptr<const SourceObservation> observation;
    PreparedScene(ID3D12Device*,const Scene&,std::shared_ptr<const SourceObservation> = {});
};
}
