#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
namespace isr {
struct ImageData {
    std::string name;
    uint32_t width{}, height{};
    std::vector<uint8_t> rgba; // Encoded source texels; color interpretation belongs to the material slot.
};
struct TextureAsset {
    std::shared_ptr<const ImageData> image;
    int minFilter = 9987, magFilter = 9729, wrapS = 10497, wrapT = 10497;
};
}
