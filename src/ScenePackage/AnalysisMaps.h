#pragma once
#include "ScenePackage/AppearanceAnchor.h"
#include "Assets/NumericDds.h"
namespace isr {
inline constexpr std::array<const char*,13> AnalysisKeys={"depth","normal","normalWorld","position","validity","region","albedo","roughness","metallic","geometryConfidence","materialConfidence","regionConfidence","tangentNormal"};
struct AnalysisMap {package::Json metadata;std::optional<NumericImage> image;};
struct AnalysisMaps {package::Json metadata;std::array<AnalysisMap,13> maps;};
std::shared_ptr<const AnalysisMaps> LoadAnalysisMaps(const std::filesystem::path&,const std::optional<package::AppearanceAnchor>&);
}
