#pragma once
#include "Scene/TextureAsset.h"
#include <span>
namespace isr {
std::shared_ptr<ImageData> DecodeImage(std::span<const uint8_t> bytes, const std::string& name);
}
