#include "ScenePackage/ScenePackageLoader.h"
#include "ScenePackage/PackageManifest.h"
#include <iostream>
#include <set>
int wmain(int argc,wchar_t** argv) {
    try {
        if(argc<2||argc>3||(argc==3&&std::wstring(argv[2])!=L"--objects")) throw std::runtime_error("Usage: ValidateScenePackage <package directory or scene.json> [--objects]");
        const auto manifest=isr::package::ReadManifest(argv[1]);
        const auto package=isr::ScenePackageLoader{}.Load(argv[1]);
        if(argc==2)std::cout << manifest.data.dump(2) << '\n';
        else {
            isr::package::Json report={{"meshCount",package.scene.meshes.size()},{"materialCount",package.scene.materials.size()},{"textureCount",package.scene.textures.size()},{"objects",isr::package::Json::array()}};
            report["materialEvidence"]=isr::package::Json::array();
            for(const auto& material:package.scene.materials) {
                const auto original=material.textures[size_t(isr::TextureRole::OriginalImage)].texture;
                report["materialEvidence"].push_back({{"name",material.name},{"albedoSource",material.albedoSource},
                    {"normalSource",material.normalSource},{"hasOriginalImage",original.has_value()},
                    {"normalStrength",material.normalScale},{"roughnessFactor",material.roughness},{"metallicFactor",material.metallic}});
            }
            for(size_t i=0;i<package.scene.entities.size();++i) {
                const auto& e=package.scene.entities[i];if(e.objectId.empty())continue;
                const auto children=package.scene.RenderableSubtree(i);std::set<size_t> materials;
                for(auto index:children)materials.insert(package.scene.entities[index].renderer->materialIndex);
                isr::package::Json item={{"id",e.objectId},{"name",e.name},{"renderers",children},{"materials",materials}};
                if(e.region){item["category"]=e.region->category;item["averageDepth"]=e.region->averageDepth?isr::package::Json(*e.region->averageDepth):isr::package::Json(nullptr);}
                report["objects"].push_back(std::move(item));
            }
            std::cout<<report.dump(2)<<'\n';
        }
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
