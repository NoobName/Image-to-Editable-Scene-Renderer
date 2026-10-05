#pragma once
#include "Scene/Scene.h"
#include "Scene/LookParameters.h"
#include <filesystem>
#include <map>
namespace isr {
struct PackageEnvironment {
    std::filesystem::path hdri;
    float intensity=1,rotation=0; // radians in memory; degrees in the manifest
    bool ibl=false,skybox=false;
};
struct ScenePackage {
    Scene scene;
    PackageEnvironment environment;
    LookParameters look;
    std::filesystem::path root;
    // Opaque analysis artifacts. No EXR decoder or AI runtime is involved.
    std::map<std::string,std::filesystem::path> auxiliary;
};
class ScenePackageLoader {
public:
    ScenePackage Load(const std::filesystem::path& packageOrManifest) const;
};
}
