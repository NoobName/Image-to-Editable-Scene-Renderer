#pragma once
#include "Scene/Transform.h"
#include "Scene/MeshRenderer.h"
#include <optional>
#include <string>
#include <array>
#include <cstdint>
namespace isr {
struct ObjectRegion {
    std::string category,namingSource,maskPath;
    uint32_t labelId=0,pixelCount=0,validDepthPixels=0,triangleCount=0;
    std::array<uint32_t,4> boundingBox{};
    std::array<uint32_t,2> imageSize{};
    std::optional<float> averageDepth;
};
struct Entity {
    std::string name;
    Transform transform;
    std::optional<MeshRenderer> renderer;
    // Parent must precede child in Scene::entities; this also rules out cycles.
    std::optional<size_t> parent;
    // Stable ScenePackage object identity lives on the logical root, not imported primitives.
    std::string objectId;
    std::optional<ObjectRegion> region;
};
}
