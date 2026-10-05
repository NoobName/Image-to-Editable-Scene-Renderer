#include "Assets/GltfSupport.h"
#include "Assets/AssetIO.h"
#include "Assets/ImageDecoder.h"
#include <map>
#include <stdexcept>
namespace isr::gltf {
void Materials(const cgltf_data& data,const std::filesystem::path& directory,Scene& scene,const std::filesystem::path& allowedRoot) {
    std::vector<std::shared_ptr<const ImageData>> images;
    std::map<std::vector<uint8_t>,std::shared_ptr<const ImageData>> decoded;
    for (size_t i=0;i<data.images_count;++i) {
        const auto& source=data.images[i]; std::vector<uint8_t> bytes;
        if (source.buffer_view) {
            const auto* start=cgltf_buffer_view_data(source.buffer_view);
            if (!start) throw std::runtime_error("Missing image bufferView data");
            bytes.assign(start,start+source.buffer_view->size);
        } else bytes=ReadImageUri(directory,source.uri,allowedRoot);
        const auto found=decoded.find(bytes);
        if (found!=decoded.end()) images.push_back(found->second);
        else {
            auto image=DecodeImage(bytes,source.name?source.name:"Image "+std::to_string(i));
            decoded.emplace(std::move(bytes),image); images.push_back(std::move(image));
        }
    }
    for (size_t i=0;i<data.textures_count;++i) {
        const auto& t=data.textures[i];
        if (!t.image) throw std::runtime_error("Texture has no PNG/JPEG image; compressed texture extensions are unsupported");
        TextureAsset texture; texture.image=images.at(size_t(t.image-data.images));
        if (t.sampler) {
            texture.minFilter=t.sampler->min_filter?t.sampler->min_filter:9987;
            texture.magFilter=t.sampler->mag_filter?t.sampler->mag_filter:9729;
            texture.wrapS=t.sampler->wrap_s; texture.wrapT=t.sampler->wrap_t;
        }
        scene.textures.push_back(std::move(texture));
    }
    auto slot=[&](const cgltf_texture_view& view) {
        TextureSlot out;
        if (!view.texture) return out;
        out.texture=size_t(view.texture-data.textures); out.texCoord=static_cast<uint32_t>(view.texcoord);
        if (view.has_transform) {
            out.offset={view.transform.offset[0],view.transform.offset[1]}; out.scale={view.transform.scale[0],view.transform.scale[1]};
            out.rotation=view.transform.rotation;
            if (view.transform.has_texcoord) out.texCoord=static_cast<uint32_t>(view.transform.texcoord);
        }
        if (out.texCoord>1) throw std::runtime_error("Only TEXCOORD_0 and TEXCOORD_1 are supported");
        return out;
    };
    for (size_t i=0;i<data.materials_count;++i) {
        const auto& source=data.materials[i]; Material m; m.name=source.name?source.name:"Material "+std::to_string(i);
        const auto& pbr=source.pbr_metallic_roughness;
        m.baseColor={pbr.base_color_factor[0],pbr.base_color_factor[1],pbr.base_color_factor[2],pbr.base_color_factor[3]};
        m.metallic=pbr.metallic_factor; m.roughness=pbr.roughness_factor;
        m.normalScale=source.normal_texture.scale; m.occlusionStrength=source.occlusion_texture.scale;
        const float strength=source.has_emissive_strength?source.emissive_strength.emissive_strength:1;
        m.emissive={source.emissive_factor[0]*strength,source.emissive_factor[1]*strength,source.emissive_factor[2]*strength};
        m.alphaMode=source.alpha_mode==cgltf_alpha_mode_blend?AlphaMode::Blend:source.alpha_mode==cgltf_alpha_mode_mask?AlphaMode::Mask:AlphaMode::Opaque;
        m.alphaCutoff=source.alpha_cutoff; m.doubleSided=source.double_sided!=0; m.unlit=source.unlit!=0;
        m.textures={slot(pbr.base_color_texture),slot(pbr.metallic_roughness_texture),slot(source.normal_texture),slot(source.emissive_texture),slot(source.occlusion_texture)};
        scene.materials.push_back(std::move(m));
    }
    Material fallback; fallback.name="glTF default"; fallback.metallic=1; fallback.roughness=1; scene.materials.push_back(fallback);
}
}
