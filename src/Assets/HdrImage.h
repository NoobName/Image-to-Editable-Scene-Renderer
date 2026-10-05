#pragma once
#include <DirectXMath.h>
#include <filesystem>
#include <vector>
namespace isr {
struct HdrImage {
    unsigned width{},height{};
    std::vector<DirectX::XMFLOAT4> pixels; // Linear RGB radiance; no gamma decoding.
};
HdrImage LoadHdr(const std::filesystem::path&);
void SaveHdr(const std::filesystem::path&,const HdrImage&);
std::vector<HdrImage> HdrMipChain(HdrImage);
}
