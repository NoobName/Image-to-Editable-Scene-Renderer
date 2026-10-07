#pragma once
#include "ScenePackage/AppearanceAnchor.h"
#include "Assets/NumericDds.h"
namespace isr {
inline constexpr std::array<const char*,8> ShadowKeys{"candidate","visibility","geometrySupport","confidence","unknown","manualConfirm","manualProtect","effectiveCandidate"};
struct ShadowData {package::Json metadata;std::array<NumericImage,8> maps;std::vector<uint32_t> excludedLabels;};
std::shared_ptr<const ShadowData> LoadShadowData(const std::filesystem::path&,const std::optional<package::AppearanceAnchor>&);
}
