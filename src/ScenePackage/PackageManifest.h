#pragma once
#include "ScenePackage/JsonSchema.h"
namespace isr::package {
struct Manifest { std::filesystem::path root; Json data; };
Manifest ReadManifest(const std::filesystem::path& packageOrManifest);
Json ReadRegion(const std::filesystem::path& root,const Json& object);
std::filesystem::path AssetPath(const std::filesystem::path& root, const std::string& relative,
    const std::string& folder, const std::vector<std::string>& extensions);
}
