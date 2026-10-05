#pragma once
#include "ScenePackage/JsonSchema.h"
#include "Scene/Scene.h"
#include <map>
namespace isr::package {
class MaterialLoader {
public:
    MaterialLoader(Scene& scene,const std::filesystem::path& root):scene_(scene),root_(root){}
    Material Load(const Json& data,const std::string& name);
    size_t Intern(const TextureAsset& texture);
private:
    size_t Texture(const std::string& path);
    Scene& scene_;
    const std::filesystem::path& root_;
    std::map<std::filesystem::path,size_t> files_;
    std::map<std::pair<size_t,size_t>,size_t> packed_;
};
}
