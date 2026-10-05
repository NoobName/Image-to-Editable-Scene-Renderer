#pragma once
#include "Scene/Scene.h"
#include <cgltf.h>
#include <filesystem>
namespace isr::gltf {
void CheckResult(cgltf_result result,const char* operation);
std::vector<float> Floats(const cgltf_accessor*,size_t components);
std::vector<uint32_t> Indices(const cgltf_accessor*,size_t vertexCount);
void Materials(const cgltf_data&,const std::filesystem::path&,Scene&,const std::filesystem::path& allowedRoot = {});
Mesh Primitive(const cgltf_primitive&,const Material&);
}
