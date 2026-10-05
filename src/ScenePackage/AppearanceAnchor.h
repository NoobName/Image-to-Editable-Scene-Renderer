#pragma once
#include "ScenePackage/JsonSchema.h"
#include <array>
#include <optional>
#include "Scene/TextureAsset.h"
namespace isr::package {
struct AppearanceAnchor {
    // Independent CPU evidence; never inserted into editable Scene::textures.
    std::filesystem::path sourcePath,analysisPath;
    std::array<uint32_t,2> sourceSize{},analysisSize{};
    Json metadata;
    std::shared_ptr<const ImageData> pixels; // Validated canonical/legacy RGB8 snapshot, retained for the 2D pass.
};
std::optional<AppearanceAnchor> ReadAppearanceAnchor(const std::filesystem::path& root);
}
