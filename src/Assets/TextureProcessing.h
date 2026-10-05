#pragma once
#include "Scene/TextureAsset.h"
namespace isr {
float SrgbToLinear(float value);
float LinearToSrgb(float value);
std::vector<ImageData> BuildMipChain(const ImageData& image,bool srgb);
}
