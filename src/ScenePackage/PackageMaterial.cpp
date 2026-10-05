#include "ScenePackage/PackageMaterial.h"
#include "ScenePackage/PackageManifest.h"
#include "Assets/AssetIO.h"
#include "Assets/ImageDecoder.h"
#include <limits>
namespace isr::package {
size_t MaterialLoader::Intern(const TextureAsset& texture) {
    auto image=texture.image;
    for(size_t i=0;i<scene_.textures.size();++i) {
        const auto& existing=scene_.textures[i];
        if(existing.image!=image && (existing.image->width!=image->width || existing.image->height!=image->height || existing.image->rgba!=image->rgba)) continue;
        image=existing.image; // Same pixels can share storage even with a different sampler or color role.
        if(existing.minFilter==texture.minFilter&&existing.magFilter==texture.magFilter&&existing.wrapS==texture.wrapS&&existing.wrapT==texture.wrapT) return i;
    }
    const size_t index=scene_.textures.size();auto stored=texture;stored.image=std::move(image);scene_.textures.push_back(std::move(stored));return index;
}
size_t MaterialLoader::Texture(const std::string& relative) {
    const auto path=AssetPath(root_,relative,"textures",{".png",".jpg",".jpeg"});
    if(const auto found=files_.find(path);found!=files_.end()) return found->second;
    TextureAsset texture;texture.image=DecodeImage(ReadAssetFile(path),relative);
    const auto index=Intern(texture);files_[path]=index;return index;
}
Material MaterialLoader::Load(const Json& data,const std::string& name) {
    Material m;m.name=name+" / package material";
    m.albedoSource=data["albedoSource"];m.normalSource=data["normalSource"];
    auto f=[](const Json& a,size_t i){return a[i].get<float>();};
    const auto& color=data["baseColorFactor"];m.baseColor={f(color,0),f(color,1),f(color,2),f(color,3)};
    const auto& e=data["emissiveFactor"];m.emissive={f(e,0),f(e,1),f(e,2)};
    m.roughness=data["roughnessFactor"];m.metallic=data["metallicFactor"];m.normalScale=data["normalStrength"];
    m.ao=data["ao"];m.occlusionStrength=data["occlusionStrength"];m.doubleSided=data["doubleSided"];m.alphaCutoff=data["alphaCutoff"];
    m.alphaMode=data["alphaMode"]=="blend"?AlphaMode::Blend:data["alphaMode"]=="mask"?AlphaMode::Mask:AlphaMode::Opaque;
    for(const auto& [key,role]:{std::pair{"baseColor",TextureRole::BaseColor},{"normal",TextureRole::Normal},{"emissive",TextureRole::Emissive},{"occlusion",TextureRole::Occlusion},{"originalImage",TextureRole::OriginalImage}})
        if(data.contains(key)) m.textures[static_cast<size_t>(role)].texture=Texture(data[key]);
    if(data.contains("roughness")||data.contains("metallic")) {
        constexpr size_t missing=std::numeric_limits<size_t>::max();
        const size_t r=data.contains("roughness")?Texture(data["roughness"]):missing, b=data.contains("metallic")?Texture(data["metallic"]):missing;
        const auto key=std::pair{r,b};size_t index;
        if(const auto found=packed_.find(key);found!=packed_.end()) index=found->second;
        else {
            const auto rough=r==missing?nullptr:scene_.textures[r].image, metal=b==missing?nullptr:scene_.textures[b].image;
            const auto source=rough?rough:metal;
            if(rough&&metal&&(rough->width!=metal->width||rough->height!=metal->height)) throw std::runtime_error("ScenePackage roughness and metallic maps must have equal dimensions");
            auto image=std::make_shared<ImageData>();image->name=name+" / packed MR";image->width=source->width;image->height=source->height;image->rgba.resize(source->rgba.size(),255);
            for(size_t p=0;p<image->rgba.size();p+=4) {image->rgba[p+1]=rough?rough->rgba[p]:255;image->rgba[p+2]=metal?metal->rgba[p]:255;}
            TextureAsset texture;texture.image=std::move(image);index=Intern(texture);packed_[key]=index;
        }
        m.textures[static_cast<size_t>(TextureRole::MetallicRoughness)].texture=index;
    }
    return m;
}
}
