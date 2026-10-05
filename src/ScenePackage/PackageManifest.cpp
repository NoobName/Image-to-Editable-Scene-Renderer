#include "ScenePackage/PackageManifest.h"
#include "Assets/AssetIO.h"
#include <set>
#include <cmath>
namespace isr::package {
std::filesystem::path AssetPath(const std::filesystem::path& root,const std::string& relative,
    const std::string& folder,const std::vector<std::string>& extensions) {
    auto bad=[&] { throw std::runtime_error("ScenePackage asset path is invalid: " + relative); };
    if(!relative.starts_with(folder+"/")) bad();
    for(unsigned char c:relative) if(c<32 || c==127 || std::string("\\:*?\"<>|").find(static_cast<char>(c))!=std::string::npos) bad();
    size_t start=0;
    do {
        const auto end=relative.find('/',start), count=(end==std::string::npos?relative.size():end)-start;
        const auto part=relative.substr(start,count);
        if(part.empty() || part=="." || part==".." || part.back()=='.' || part.back()==' ') bad();
        if(end==std::string::npos) break;
        start=end+1;
    } while(true);
    const auto path=std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(relative.data()),relative.size()));
    auto extension=PathUtf8(path.extension());
    std::transform(extension.begin(),extension.end(),extension.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
    if(std::find(extensions.begin(),extensions.end(),extension)==extensions.end()) bad();
    return ConstrainAssetPath(root/path,root);
}
Json ReadRegion(const std::filesystem::path& root,const Json& object) {
    auto region=ReadJson(AssetPath(root,object["region"],"objects",{".json"}));
    Validate(region,Schema()["$defs"]["region"],"region");
    if(!object.contains("id")||region["id"]!=object["id"]||region["name"]!=object["name"])
        throw std::runtime_error("Region id/name must match its scene object");
    AssetPath(root,region["mask"],"objects",{".png"});
    const auto& b=region["boundingBox"];const auto& size=region["imageSize"];
    const auto x=b[0].get<uint32_t>(),y=b[1].get<uint32_t>(),w=b[2].get<uint32_t>(),h=b[3].get<uint32_t>();
    if(!w||!h||x+w>size[0].get<uint32_t>()||y+h>size[1].get<uint32_t>()||region["pixelCount"].get<uint64_t>()>uint64_t(w)*h||region["validDepthPixels"].get<uint32_t>()>region["pixelCount"].get<uint32_t>())
        throw std::runtime_error("Invalid region bounds or pixel counts");
    if(region["averageDepth"].is_null()!=(region["validDepthPixels"].get<uint32_t>()==0))
        throw std::runtime_error("Region averageDepth is null exactly when validDepthPixels is zero");
    if(object.contains("mesh")!=(region["triangleCount"].get<uint32_t>()>0))
        throw std::runtime_error("Region triangleCount and mesh presence disagree");
    return region;
}
Manifest ReadManifest(const std::filesystem::path& input) {
    auto path=std::filesystem::is_directory(input)?input/"scene.json":input;
    if(path.filename()!=L"scene.json") throw std::runtime_error("ScenePackage expects a directory or scene.json");
    // Keep the selected directory as the boundary even when scene.json itself is a symlink.
    std::error_code error;
    const auto root=std::filesystem::canonical(path.parent_path().empty()?std::filesystem::path("."):path.parent_path(),error);
    if(error) throw std::runtime_error("Cannot resolve ScenePackage directory: "+PathUtf8(path.parent_path()));
    path=ConstrainAssetPath(path,root);
    for(const char* name:{"meshes","textures","masks","debug"})
        if(!std::filesystem::is_directory(ConstrainAssetPath(root/name,root,false))) throw std::runtime_error(std::string("ScenePackage missing directory: ")+name);
    auto data=ReadJson(path); Validate(data,Schema()); ApplyDefaults(data,Schema()); Validate(data,Schema());
    const auto& c=data["camera"];
    if(c["far"].get<double>()<=c["near"].get<double>()) throw std::runtime_error("ScenePackage $.camera: far must exceed near");
    double distance=0,horizontal=0;
    for(size_t i=0;i<3;++i) { const double d=c["target"][i].get<double>()-c["position"][i].get<double>(); distance+=d*d; if(i!=1)horizontal+=d*d; }
    if(distance<1e-8 || horizontal/distance<std::sin(0.01)*std::sin(0.01))
        throw std::runtime_error("ScenePackage $.camera: target must differ from position; pitch must stay within camera limits (+/-89.427 degrees)");
    const auto& env=data["environment"];
    if(env.contains("hdri")) AssetPath(root,env["hdri"],"textures",{".hdr"});
    for(const auto& light:data["lights"]) if(light["type"]=="directional") {
        double norm=0;for(const auto& v:light["direction"])norm+=v.get<double>()*v.get<double>();
        if(norm<1e-8) throw std::runtime_error("ScenePackage directional light needs a nonzero direction");
    }
    std::set<std::string> names,ids;std::set<uint32_t> labelIds;
    for(const auto& object:data["objects"]) {
        if(!names.insert(object["name"].get<std::string>()).second) throw std::runtime_error("ScenePackage duplicate object name: "+object["name"].get<std::string>());
        if(!ids.insert(object.value("id","legacy:"+object["name"].get<std::string>())).second)throw std::runtime_error("ScenePackage duplicate object ID");
        if(object.contains("region")) {
            const auto region=ReadRegion(root,object);
            if(!labelIds.insert(region["labelId"].get<uint32_t>()).second)throw std::runtime_error("Duplicate region label ID");
        }else if(!object.contains("mesh"))throw std::runtime_error("Object needs a mesh or region metadata");
        if(object.contains("mesh")) {
            const auto mesh=object["mesh"].get<std::string>();
            AssetPath(root,mesh,mesh.starts_with("objects/")?"objects":"meshes",{".gltf",".glb"});
        }
        for(const auto& s:object["transform"]["scale"]) if(std::abs(s.get<double>())<=0.0001)
            throw std::runtime_error("ScenePackage object scale must have abs(component) > 0.0001");
        if(object.contains("material")) for(const char* key:{"baseColor","normal","roughness","metallic","emissive","occlusion","originalImage","confidence"})
            if(object["material"].contains(key)) AssetPath(root,object["material"][key],"textures",{".png",".jpg",".jpeg"});
    }
    for(auto it=data["auxiliary"].begin();it!=data["auxiliary"].end();++it) {
        const bool mask=it.key()=="segmentation";
        AssetPath(root,it.value(),mask?"masks":"debug",{mask?".png":".exr"});
    }
    return {root,std::move(data)};
}
}
