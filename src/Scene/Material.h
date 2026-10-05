#pragma once
#include <DirectXMath.h>
#include <string>
#include <array>
#include <optional>
namespace isr {
enum class TextureRole : size_t { BaseColor, MetallicRoughness, Normal, Emissive, Occlusion, OriginalImage, Count };
constexpr size_t MaterialTextureCount = static_cast<size_t>(TextureRole::Count);
constexpr bool IsSrgb(TextureRole role) { return role == TextureRole::BaseColor || role == TextureRole::Emissive || role == TextureRole::OriginalImage; }
struct TextureSlot {
    std::optional<size_t> texture;
    uint32_t texCoord = 0;
    DirectX::XMFLOAT2 offset{0,0}, scale{1,1};
    float rotation = 0;
};
enum class AlphaMode { Opaque, Mask, Blend };
struct Material {
    std::string name;
    DirectX::XMFLOAT4 baseColor{1,1,1,1}; // All factors are linear.
    float metallic = 0, roughness = 0.5f, ao = 1, normalScale = 1;
    float occlusionStrength = 1;
    DirectX::XMFLOAT3 emissive{0,0,0};
    float alphaCutoff = 0.5f;
    AlphaMode alphaMode = AlphaMode::Opaque;
    bool doubleSided = false, unlit = false;
    std::array<TextureSlot, MaterialTextureCount> textures;
    std::string albedoSource="authored",normalSource="authored";
};
}
