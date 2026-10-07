#pragma once
#include "ScenePackage/LightingData.h"
#include "Assets/NumericDds.h"
#include <optional>
namespace isr {
struct ReferenceAnalysis {
    package::Json metadata;
    LightingParameters target,sourceBaseline;
    std::array<std::shared_ptr<const ImageData>,2> previews;
    std::filesystem::path root;
    float confidence{};
    bool canApply=false;
    package::Json optimization;
    std::optional<NumericImage> candidate;
};
struct ReferenceSession {
    std::shared_ptr<const ReferenceAnalysis> analysis;
    LightingParameters previousTarget;
    float previousGain=1,previousExposure=0;
    bool applied=false,showResidual=false;
    bool gpuVerified=false;
    double gpuError=0;
};
class RelightingSession;
std::shared_ptr<const ReferenceAnalysis> LoadReferenceAnalysis(const std::filesystem::path&);
void CheckReferenceSource(const ReferenceAnalysis&,const RelightingSession&);
bool ApplyReferenceProposal(RelightingSession&);
bool ResetReferenceTarget(RelightingSession&);
}
