#pragma once
#include "ScenePackage/AppearanceAnchor.h"
#include "ScenePackage/AnalysisMaps.h"
#include "ScenePackage/LightingData.h"
#include <map>
namespace isr {
// Published through shared_ptr<const>; editing a Scene never writes source evidence.
struct SourceObservation {
    std::filesystem::path packageRoot;
    std::optional<package::AppearanceAnchor> anchor;
    std::map<std::string,std::filesystem::path> analysisArtifacts;
    package::Json analysisMetadata;
    std::string analysisMetadataDiagnostic;
    std::shared_ptr<const AnalysisMaps> analysisMaps;
    std::shared_ptr<const LightingData> lighting;
    bool CanDisplayImage()const {return anchor&&anchor->pixels!=nullptr;}
};
}
