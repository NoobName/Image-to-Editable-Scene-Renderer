#pragma once
#include "Renderer/ScenePass.h"
#include "Renderer/SourceImagePass.h"
#include "Renderer/AnalysisTextures.h"
#include "Renderer/LightingPreview.h"
#include "Renderer/IntrinsicPreview.h"
#include "Renderer/ShadowPreview.h"
#include "Renderer/ImageRelightingRenderer.h"
#include "Renderer/ImageRelightingComposite.h"
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
    std::unique_ptr<IntrinsicPreview> intrinsicPreview;
    std::unique_ptr<ShadowPreview> shadowPreview;
    std::unique_ptr<ImageRelightingRenderer> imageRelighting;
    std::unique_ptr<ImageRelightingComposite> imageComposite;
    std::shared_ptr<const SourceObservation> observation;
    std::shared_ptr<const ProtectionMask> importedProtection;
    PreparedScene(ID3D12Device*,const Scene&,std::shared_ptr<const SourceObservation> = {},std::shared_ptr<const ProtectionMask> = {});
};
}
