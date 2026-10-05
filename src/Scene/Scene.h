#pragma once
#include "Scene/Camera.h"
#include "Scene/Entity.h"
#include "Scene/Mesh.h"
#include "Scene/Material.h"
#include "Scene/Light.h"
#include "Scene/TextureAsset.h"
namespace isr {
struct Scene {
    Camera camera;
    std::vector<Mesh> meshes;
    std::vector<Material> materials;
    std::vector<Entity> entities;
    std::vector<Light> lights;
    std::vector<TextureAsset> textures;
    void UpdateWorldMatrices();
    std::vector<size_t> RenderableSubtree(size_t root) const;
    static Scene CreateDemo();
};
}
