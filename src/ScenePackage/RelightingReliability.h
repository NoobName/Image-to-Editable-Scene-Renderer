#pragma once
#include "ScenePackage/SourceObservation.h"
namespace isr {
struct ProtectionMask {NumericImage image;package::Json metadata;};
std::shared_ptr<const ProtectionMask> LoadProtectionMask(const std::filesystem::path&);
struct RelightingReliability {
    // A: geometry/normal, boundary, material, fit. B: shadow-risk, intrinsic support, residual fraction, available.
    std::array<NumericImage,2> weights;
    package::Json provenance;
};
RelightingReliability BuildRelightingReliability(const AnalysisMaps*,const LightingData*,uint32_t width,uint32_t height,const IntrinsicData* = nullptr);
}
