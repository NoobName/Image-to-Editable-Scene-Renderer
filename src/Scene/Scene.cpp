#include "Scene/Scene.h"
#include <stdexcept>
namespace isr {
std::vector<size_t> Scene::RenderableSubtree(size_t root) const {
    if(root>=entities.size())throw std::out_of_range("Invalid scene object index");
    std::vector<size_t> result;
    for(size_t i=root;i<entities.size();++i)if(entities[i].renderer) {
        size_t ancestor=i;
        while(ancestor>root&&entities[ancestor].parent)ancestor=*entities[ancestor].parent;
        if(ancestor==root)result.push_back(i);
    }
    return result;
}
void Scene::UpdateWorldMatrices() {
    for (size_t i = 0; i < entities.size(); ++i) {
        auto& entity = entities[i];
        if (entity.parent && *entity.parent >= i) throw std::invalid_argument("Parent must precede child");
        if (entity.renderer && (entity.renderer->meshIndex >= meshes.size() || entity.renderer->materialIndex >= materials.size()))
            throw std::out_of_range("Invalid scene asset reference");
        entity.transform.UpdateWorld(entity.parent ? entities[*entity.parent].transform.WorldMatrix() : DirectX::XMMatrixIdentity());
    }
}
Scene Scene::CreateDemo() {
    Scene scene; scene.meshes = {Mesh::Cube(), Mesh::Sphere(), Mesh::Plane()};
    scene.materials = {{"Terracotta",{0.9f,0.29f,0.16f,1}},{"Jade",{0.13f,0.66f,0.56f,1}},{"Slate",{0.27f,0.32f,0.4f,1}}};
    Entity cube; cube.name = "Cube"; cube.transform.position = {-1.35f,0.75f,0};
    cube.transform.scale = {1.5f,1.5f,1.5f}; cube.transform.rotation.y = 0.35f; cube.renderer = MeshRenderer{0,0};
    Entity sphere; sphere.name = "Sphere"; sphere.transform.position = {1.2f,1.0f,0}; sphere.renderer = MeshRenderer{1,1};
    Entity plane; plane.name = "Plane"; plane.transform.scale = {10,1,10}; plane.renderer = MeshRenderer{2,2};
    // Draw ground last: this scene actively relies on depth testing.
    scene.materials[0].roughness=0.4f;scene.materials[1].metallic=0.85f;scene.materials[1].roughness=0.22f;scene.materials[2].roughness=0.85f;
    scene.entities = {cube,sphere,plane}; scene.lights.push_back(Light{});
    Light point;point.type=LightType::Point;point.position={2,3,-3};point.color={1,0.7f,0.4f};point.intensity=45;point.range=15;scene.lights.push_back(point);
    scene.UpdateWorldMatrices(); return scene;
}
}
