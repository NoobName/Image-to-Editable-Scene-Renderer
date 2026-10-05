#pragma once
#include "Scene/TextureAsset.h"
#include <span>
namespace isr {
std::shared_ptr<ImageData> DecodeImage(std::span<const uint8_t> bytes, const std::string& name,uint64_t maxPixels=128ull*1024*1024);
}
