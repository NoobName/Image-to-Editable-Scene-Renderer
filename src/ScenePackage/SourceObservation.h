#pragma once
#include "ScenePackage/AppearanceAnchor.h"
#include "ScenePackage/AnalysisMaps.h"
#include "ScenePackage/LightingData.h"
#include "ScenePackage/IntrinsicData.h"
#include "ScenePackage/ShadowData.h"
#include <map>
namespace isr {
// Published through shared_ptr<const>; editing a Scene never writes source evidence.
struct SourceObservation {
    struct Region {uint32_t label;std::string id,name,category;};
    std::vector<Region> regions; // Immutable identities captured at package load, independent of editable entities.
    std::filesystem::path packageRoot;
    std::optional<package::AppearanceAnchor> anchor;
    std::map<std::string,std::filesystem::path> analysisArtifacts;
    package::Json analysisMetadata;
    std::string analysisMetadataDiagnostic;
    std::shared_ptr<const AnalysisMaps> analysisMaps;
    std::shared_ptr<const LightingData> lighting;
    std::shared_ptr<const IntrinsicData> intrinsic;
    std::shared_ptr<const ShadowData> shadow;
    bool CanDisplayImage()const {return anchor&&anchor->pixels!=nullptr;}
};
}
