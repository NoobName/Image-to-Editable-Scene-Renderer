#include "ScenePackage/ScenePackageLoader.h"
#include "ScenePackage/PackageManifest.h"
#include "ScenePackage/PackageMaterial.h"
#include "Assets/ModelLoader.h"
#include "Assets/MeshProcessing.h"
#include "Core/Log.h"
#include <map>
namespace isr {
namespace {
DirectX::XMFLOAT3 Vec3(const package::Json& v) {return {v[0].get<float>(),v[1].get<float>(),v[2].get<float>()};}
}
ScenePackage ScenePackageLoader::Load(const std::filesystem::path& path) const {
    const auto manifest=package::ReadManifest(path);const auto& data=manifest.data;
    ScenePackage result;result.root=manifest.root;auto& scene=result.scene;
    result.appearance=package::ReadAppearanceAnchor(result.root);
    Log(result.appearance?"AppearanceAnchor: validated immutable CPU image and metadata":"AppearanceAnchor: absent; loading legacy 3D scene");
    const auto& c=data["camera"];
    scene.camera.LookAt(Vec3(c["position"]),Vec3(c["target"]));
    scene.camera.SetPerspective(c["fovYDegrees"].get<float>()*DirectX::XM_PI/180,c["aspect"],c["near"],c["far"]);
    const auto& env=data["environment"];
    if(env.contains("hdri")) {
        result.environment.hdri=package::AssetPath(result.root,env["hdri"],"textures",{".hdr"});
        result.environment.ibl=env["ibl"];result.environment.skybox=env["skybox"];
    }
    result.environment.intensity=env["intensity"];result.environment.rotation=env["rotationDegrees"].get<float>()*DirectX::XM_PI/180;
    const auto& look=data["look"];
    for(const auto& p:LookParameterSchema) result.look.*(p.member)=look.at(std::string(p.key)).get<float>();
    result.look.bloomEnabled=look["bloom"];
    result.look.toneMapping=look["tone-mapping"]=="none"?ToneMapping::None:look["tone-mapping"]=="reinhard"?ToneMapping::Reinhard:ToneMapping::ACES;
    ValidateLookParameters(result.look);
    for(const auto& value:data["lights"]) {
        Light light;light.type=value["type"]=="point"?LightType::Point:LightType::Directional;
        if(light.type==LightType::Point) {light.position=Vec3(value["position"]);light.range=value["range"];}
        else light.direction=Vec3(value["direction"]);
        light.color=Vec3(value["color"]);light.intensity=value["intensity"];scene.lights.push_back(light);
    }
    package::MaterialLoader materials(scene,result.root);
    std::map<std::filesystem::path,Scene> models;
    std::map<std::pair<std::filesystem::path,bool>,size_t> meshOffsets;
    for(const auto& object:data["objects"]) {
        Entity root;root.name=object["name"];root.objectId=object.value("id","legacy:"+root.name);
        const auto& t=object["transform"];
        root.transform.position=Vec3(t["position"]);root.transform.scale=Vec3(t["scale"]);root.transform.rotation=Vec3(t["rotationDegrees"]);
        root.transform.rotation.x*=DirectX::XM_PI/180;root.transform.rotation.y*=DirectX::XM_PI/180;root.transform.rotation.z*=DirectX::XM_PI/180;
        if(object.contains("region")) {
            const auto r=package::ReadRegion(result.root,object);ObjectRegion info;
            info.category=r["category"];info.namingSource=r["namingSource"];info.maskPath=r["mask"];
            info.labelId=r["labelId"];info.pixelCount=r["pixelCount"];info.validDepthPixels=r["validDepthPixels"];info.triangleCount=r["triangleCount"];
            info.boundingBox=r["boundingBox"].get<std::array<uint32_t,4>>();info.imageSize=r["imageSize"].get<std::array<uint32_t,2>>();
            if(!r["averageDepth"].is_null())info.averageDepth=r["averageDepth"].get<float>();root.region=std::move(info);
        }
        const size_t rootIndex=scene.entities.size();scene.entities.push_back(std::move(root));
        if(!object.contains("mesh"))continue; // A segmented sky may have no observed 3D surface.
        const auto mesh=object["mesh"].get<std::string>();
        const auto modelPath=package::AssetPath(result.root,mesh,mesh.starts_with("objects/")?"objects":"meshes",{".gltf",".glb"});
        auto found=models.find(modelPath);
        if(found==models.end()) found=models.emplace(modelPath,ModelLoader{}.Load(modelPath,result.root)).first;
        const auto& model=found->second;
        const bool overrideMaterial=object.contains("material"), rebuildTangents=overrideMaterial&&object["material"].contains("normal");
        const auto meshKey=std::pair{modelPath,rebuildTangents};
        if(!meshOffsets.contains(meshKey)) {
            meshOffsets[meshKey]=scene.meshes.size();
            for(auto mesh:model.meshes) {if(rebuildTangents)GenerateTangents(mesh,0);scene.meshes.push_back(std::move(mesh));}
        }
        const size_t meshOffset=meshOffsets.at(meshKey), materialOffset=scene.materials.size();
        if(overrideMaterial) scene.materials.push_back(materials.Load(object["material"],object["name"]));
        else {
            std::vector<size_t> textures;for(const auto& texture:model.textures)textures.push_back(materials.Intern(texture));
            for(auto material:model.materials) {
                for(auto& slot:material.textures) if(slot.texture) slot.texture=textures.at(*slot.texture);
                scene.materials.push_back(std::move(material));
            }
        }
        const size_t entityOffset=scene.entities.size();
        for(auto entity:model.entities) {
            entity.parent=entity.parent?entityOffset+*entity.parent:rootIndex;
            if(entity.renderer) {
                entity.renderer->meshIndex+=meshOffset;
                entity.renderer->materialIndex=materialOffset+(overrideMaterial?0:entity.renderer->materialIndex);
                entity.renderer->visible=object["visible"];
            }
            scene.entities.push_back(std::move(entity));
        }
    }
    for(auto it=data["auxiliary"].begin();it!=data["auxiliary"].end();++it) {
        const bool mask=it.key()=="segmentation";
        result.auxiliary[it.key()]=package::AssetPath(result.root,it.value(),mask?"masks":"debug",{mask?".png":".exr"});
    }
    auto observation=std::make_shared<SourceObservation>();observation->packageRoot=result.root;
    observation->anchor=result.appearance;observation->analysisArtifacts=result.auxiliary;
    const auto report=result.root/"debug/reconstruction.json";
    if(std::filesystem::exists(report)){
        // This diagnostic report is not part of the v1/anchor contract. A broken old report
        // must not turn an otherwise valid legacy scene into an unloadable package.
        try{
            auto metadata=package::ReadJson(package::AssetPath(result.root,"debug/reconstruction.json","debug",{".json"}));
            if(!metadata.is_object())throw std::runtime_error("expected a JSON object");
            observation->analysisMetadata=std::move(metadata);
        }catch(const std::exception& error){
            observation->analysisMetadataDiagnostic=std::string("Optional debug/reconstruction.json ignored: ")+error.what();
            Log(observation->analysisMetadataDiagnostic);
        }
    }
    observation->analysisMaps=LoadAnalysisMaps(result.root,result.appearance);
    observation->lighting=LoadLightingData(result.root,result.appearance);
    result.observation=std::move(observation);
    scene.UpdateWorldMatrices();
    Log("Loaded ScenePackage v1: objects="+std::to_string(data["objects"].size())+" lights="+std::to_string(scene.lights.size())+" auxiliary="+std::to_string(result.auxiliary.size()));
    return result; // All validation and CPU reconstruction complete before publishing the scene.
}
}
