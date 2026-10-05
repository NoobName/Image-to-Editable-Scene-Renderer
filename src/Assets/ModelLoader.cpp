#include "Assets/ModelLoader.h"
#include "Assets/GltfSupport.h"
#include "Assets/AssetIO.h"
#include "Core/Log.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <map>
#include <set>
#include <limits>
namespace isr {
using namespace DirectX;
Scene ModelLoader::Load(const std::filesystem::path& path,const std::filesystem::path& allowedRoot) const {
    cgltf_options options{};
    options.file.user_data=const_cast<std::filesystem::path*>(&allowedRoot);
    options.file.read=[](const cgltf_memory_options*,const cgltf_file_options* file,const char* name,cgltf_size* size,void** data) {
        try {
            auto path=std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(name)));
            auto bytes=ReadAssetFile(ConstrainAssetPath(path,*static_cast<const std::filesystem::path*>(file->user_data)));
            if(*size && bytes.size()<*size)return cgltf_result_data_too_short;
            *data=std::malloc(std::max<size_t>(bytes.size(),1)); if(!*data)return cgltf_result_out_of_memory;
            std::memcpy(*data,bytes.data(),bytes.size()); *size=bytes.size(); return cgltf_result_success;
        } catch (...) { return cgltf_result_io_error; }
    };
    options.file.release=[](const cgltf_memory_options*,const cgltf_file_options*,void* data){std::free(data);};
    cgltf_data* parsed=nullptr; const auto utf8=PathUtf8(path);
    gltf::CheckResult(cgltf_parse_file(&options,utf8.c_str(),&parsed),"Parse glTF");
    std::unique_ptr<cgltf_data,decltype(&cgltf_free)> data(parsed,cgltf_free);
    const std::set<std::string> supported{"KHR_texture_transform","KHR_materials_unlit","KHR_materials_emissive_strength","KHR_mesh_quantization"};
    for(size_t i=0;i<data->extensions_required_count;++i)
        if(!supported.contains(data->extensions_required[i]))throw std::runtime_error(std::string("Unsupported required extension: ")+data->extensions_required[i]);
    for(size_t i=0;i<data->buffers_count;++i) if(data->buffers[i].uri) {
        std::string uri=data->buffers[i].uri;
        if(!uri.starts_with("data:") && uri.find(':')!=std::string::npos)throw std::runtime_error("External buffers must use relative local URIs");
    }
    gltf::CheckResult(cgltf_load_buffers(&options,data.get(),utf8.c_str()),"Load glTF buffers");
    gltf::CheckResult(cgltf_validate(data.get()),"Validate glTF");
    Scene scene; gltf::Materials(*data,path.parent_path(),scene,allowedRoot);
    std::map<const cgltf_primitive*,size_t> meshCache;
    const auto reflection=XMMatrixScaling(1,1,-1);
    std::set<const cgltf_node*> active;
    std::function<void(const cgltf_node*,std::optional<size_t>)> visit;
    visit=[&](const cgltf_node* node,std::optional<size_t> parent) {
        if(active.size()>256 || !active.insert(node).second)throw std::runtime_error("Cyclic or excessively deep node graph");
        if(node->skin)throw std::runtime_error("Skinned nodes are unsupported");
        Entity entity; entity.name=node->name?node->name:"Node"; entity.parent=parent;
        XMFLOAT4X4 local{}; cgltf_node_transform_local(node,&local._11);
        XMStoreFloat4x4(&local,reflection*XMLoadFloat4x4(&local)*reflection); entity.transform.importedLocal=local;
        const size_t index=scene.entities.size(); scene.entities.push_back(entity);
        if(node->mesh) for(size_t i=0;i<node->mesh->primitives_count;++i) {
            const auto* primitive=&node->mesh->primitives[i];
            const size_t material=primitive->material?size_t(primitive->material-data->materials):scene.materials.size()-1;
            auto found=meshCache.find(primitive); size_t meshIndex;
            if(found==meshCache.end()) { meshIndex=scene.meshes.size(); scene.meshes.push_back(gltf::Primitive(*primitive,scene.materials.at(material))); meshCache[primitive]=meshIndex; }
            else meshIndex=found->second;
            Entity drawable; drawable.name=entity.name+" / primitive "+std::to_string(i); drawable.parent=index;
            drawable.renderer=MeshRenderer{meshIndex,material}; scene.entities.push_back(std::move(drawable));
        }
        for(size_t i=0;i<node->children_count;++i)visit(node->children[i],index);
        active.erase(node);
    };
    const auto* selected=data->scene?data->scene:(data->scenes_count?&data->scenes[0]:nullptr);
    if(selected)for(size_t i=0;i<selected->nodes_count;++i)visit(selected->nodes[i],{});
    else for(size_t i=0;i<data->nodes_count;++i)if(!data->nodes[i].parent)visit(&data->nodes[i],{});
    if(scene.meshes.empty())throw std::runtime_error("glTF scene contains no renderable triangles");
    scene.UpdateWorldMatrices();
    auto minimum=XMVectorReplicate(std::numeric_limits<float>::max()),maximum=-minimum;
    for(const auto& e:scene.entities)if(e.renderer)for(const auto& v:scene.meshes[e.renderer->meshIndex].vertices) {
        auto p=XMVector3TransformCoord(XMLoadFloat3(&v.position),e.transform.WorldMatrix()); minimum=XMVectorMin(minimum,p);maximum=XMVectorMax(maximum,p);
    }
    const auto center=(minimum+maximum)*0.5f;
    const float radius=std::max(0.1f,XMVectorGetX(XMVector3Length(maximum-minimum))*0.5f);
    XMFLOAT3 target,eye;XMStoreFloat3(&target,center);XMStoreFloat3(&eye,center+XMVectorSet(0.7f,0.45f,-1.6f,0)*radius);
    scene.camera.LookAt(eye,target);scene.camera.SetPerspective(XM_PIDIV4,16.0f/9.0f,std::max(0.001f,radius/1000),radius*30);
    scene.lights.push_back(Light{});
    Light point;point.type=LightType::Point;point.position={target.x+radius*0.35f,target.y+radius*0.6f,target.z-radius*0.6f};
    point.color={1,0.75f,0.5f};point.intensity=radius*radius*3;point.range=radius*4;scene.lights.push_back(point);
    Log("Loaded glTF: "+utf8+"; meshes="+std::to_string(scene.meshes.size())+" materials="+std::to_string(scene.materials.size())+" textures="+std::to_string(scene.textures.size()));
    return scene;
}
}
