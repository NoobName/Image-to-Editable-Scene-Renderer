#pragma once
#include "ScenePackage/JsonSchema.h"
#include "Scene/TextureAsset.h"
#include <span>
namespace isr::package {
std::string Sha256(std::span<const uint8_t>);
std::filesystem::path ValidateAnchorImage(const std::filesystem::path& root,const Json& record,bool original=false,
    std::shared_ptr<const ImageData>* retainPixels=nullptr);
}
