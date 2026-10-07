#pragma once
#include "ScenePackage/AppearanceAnchor.h"
#include "Assets/NumericDds.h"
namespace isr {
inline constexpr std::array<const char*,6> IntrinsicKeys{"albedo","shading","residual","uncertainty","error","validity"};
struct IntrinsicData {package::Json metadata;std::array<std::optional<NumericImage>,6> maps;};
std::shared_ptr<const IntrinsicData> LoadIntrinsicData(const std::filesystem::path&,const std::optional<package::AppearanceAnchor>&);
}
