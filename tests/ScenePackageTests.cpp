#include "ScenePackage/ScenePackageLoader.h"
#include "ScenePackage/PackageManifest.h"
#include "ScenePackage/PackageMaterial.h"
#include "Assets/ModelLoader.h"
#include <iostream>
#include <cmath>
#include <fstream>
#include <chrono>
using namespace isr;
namespace {
int checks=0;
void Require(bool value,const char* reason) {++checks;if(!value)throw std::runtime_error(reason);}
bool Near(float a,float b) {return std::abs(a-b)<1e-4f;}
}
int main() {try {
    const std::filesystem::path root=PACKAGE_FIXTURE_DIR;
    auto loaded=ScenePackageLoader{}.Load(root);
    Require(!loaded.appearance,"Legacy fixture unexpectedly acquired source-image provenance");
    auto& scene=loaded.scene;
    Require(scene.lights.size()==2,"glTF default lights leaked into package");
    Require(Near(scene.camera.Position().x,8)&&Near(scene.camera.FarPlane(),200),"Package camera was replaced by glTF auto-framing");
    Require(Near(loaded.look.exposure,.3f)&&loaded.look.bloomEnabled&&Near(loaded.look.bloomIntensity,.12f),"Look parameters lost");
    Require(loaded.environment.ibl&&loaded.environment.skybox&&Near(loaded.environment.intensity,.7f),"Environment settings lost");
    Require(Near(loaded.environment.rotation,25*DirectX::XM_PI/180),"Environment degrees conversion failed");
    Require(loaded.auxiliary.contains("segmentation"),"Auxiliary reference lost");
    Require(scene.meshes.size()==6&&scene.materials.size()==5,"Model merge or per-object material replacement failed");
    const auto& material=scene.materials.back();
    Require(Near(material.normalScale,.6f)&&Near(material.metallic,1)&&Near(material.roughness,1),"Material factors/defaults failed");
    const auto& packed=scene.textures.at(*material.textures[size_t(TextureRole::MetallicRoughness)].texture).image;
    Require(packed->width==1&&packed->height==1&&packed->rgba==std::vector<uint8_t>({255,90,220,255}),"Roughness R -> G / metallic R -> B packing failed");
    const auto normalIndex=*material.textures[size_t(TextureRole::Normal)].texture;
    Require(normalIndex<4,"Same external/embedded normal texture did not reuse storage");
    Require(scene.textures.size()==8,"Unexpected duplicate texture storage");
    {
        Scene scratch;package::MaterialLoader loader(scratch,root);
        auto value=package::ReadManifest(root).data["objects"][1]["material"];
        value.erase("roughness");
        const auto first=loader.Load(value,"first"), second=loader.Load(value,"second");
        const auto slot=size_t(TextureRole::MetallicRoughness);
        Require(first.textures[slot].texture==second.textures[slot].texture,"Repeated MR pair did not reuse packed texture");
        Require(scratch.textures.at(*first.textures[slot].texture).image->rgba==std::vector<uint8_t>({255,255,220,255}),"Missing roughness must contribute white");
        value.erase("metallic");const auto noMaps=loader.Load(value,"plain");
        Require(!noMaps.textures[slot].texture,"Absent scalar maps unexpectedly created MR texture");
    }
    const auto model=ModelLoader{}.Load(root/"meshes/MaterialLab.glb");
    const size_t stride=1+model.entities.size();
    Require(scene.entities.size()==stride*2,"Object container hierarchy was flattened or lost");
    for(size_t object=0;object<2;++object) {
        const size_t start=object*stride;Require(!scene.entities[start].parent,"Object root unexpectedly has a parent");
        for(size_t i=0;i<model.entities.size();++i) {
            const auto& entity=scene.entities[start+1+i];
            Require(entity.parent==std::optional<size_t>(model.entities[i].parent?start+1+*model.entities[i].parent:start),"Entity parent index remapping failed");
            DirectX::XMFLOAT4X4 expected,actual;
            DirectX::XMStoreFloat4x4(&expected,model.entities[i].transform.WorldMatrix()*scene.entities[start].transform.WorldMatrix());
            DirectX::XMStoreFloat4x4(&actual,entity.transform.WorldMatrix());
            for(int r=0;r<4;++r)for(int col=0;col<4;++col)Require(Near(expected.m[r][col],actual.m[r][col]),"Node/world transform composition failed");
            if(entity.renderer)Require(entity.renderer->materialIndex==(object?4:model.entities[i].renderer->materialIndex),"Material index remapping failed");
        }
    }
    // Ensure the interchange schema cannot drift from the existing UI/CLI range contract.
    const auto& rules=package::Schema()["$defs"]["look"]["properties"];LookParameters defaults;
    for(const auto& p:LookParameterSchema) {
        const auto& rule=rules.at(std::string(p.key));
        Require(Near(rule["minimum"].get<float>(),p.minimum)&&Near(rule["maximum"].get<float>(),p.maximum),"Schema/UI look bounds drifted");
        Require(Near(rule["default"].get<float>(),defaults.*(p.member)),"Schema/UI look defaults drifted");
    }
    auto again=ScenePackageLoader{}.Load(root/"scene.json");scene.materials.back().metallic=.123f;
    Require(Near(again.scene.materials.back().metallic,1),"Independent load shares mutable material state");
    // Transactional load: validation must finish before assignment replaces the edited scene.
    const auto temporary=std::filesystem::temp_directory_path()/("isr-anchor-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    struct Cleanup{std::filesystem::path path;~Cleanup(){std::error_code error;std::filesystem::remove_all(path,error);}}cleanup{temporary};
    std::filesystem::copy(root,temporary,std::filesystem::copy_options::recursive);
    std::filesystem::create_directories(temporary/"debug");
    {std::ofstream file(temporary/"debug/reconstruction.json");file<<"not a valid optional report";}
    const auto legacy=ScenePackageLoader{}.Load(temporary);
    Require(!legacy.observation->analysisMetadataDiagnostic.empty()&&legacy.observation->analysisMetadata.is_null(),"Optional report failure lacks a diagnostic");
    Require(legacy.scene.entities.size()==again.scene.entities.size()&&!legacy.observation->CanDisplayImage(),"Broken optional report prevented legacy 3D loading");
    std::filesystem::create_directories(temporary/"relighting");
    {std::ofstream file(temporary/"relighting/relighting.json");file<<R"({"version":999})";}
    again.scene.materials.back().metallic=.456f;const auto previousObservation=again.observation;bool rejected=false;
    try{again=ScenePackageLoader{}.Load(temporary);}catch(const std::exception& error){rejected=std::string(error.what()).find("relighting/relighting.json")!=std::string::npos;}
    Require(rejected&&Near(again.scene.materials.back().metallic,.456f),"Damaged extension replaced current scene or lacked diagnostic");
    Require(again.observation==previousObservation,"Damaged extension replaced the current source observation");
    std::cout<<"ScenePackage: "<<checks<<" checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
