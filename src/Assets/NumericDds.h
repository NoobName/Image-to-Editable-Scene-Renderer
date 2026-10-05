#pragma once
#include <filesystem>
#include <vector>
#include <cstdint>
namespace isr {
// Values deliberately match DXGI, but this CPU loader has no device or model dependency.
enum class NumericFormat:uint32_t { Vector=2, Float=41, Label=42 };
struct NumericImage {
    uint32_t width{},height{};
    NumericFormat format{};
    std::vector<uint8_t> bytes;
    unsigned Channels()const{return format==NumericFormat::Vector?4:1;}
    float FloatAt(size_t pixel,unsigned channel=0)const;
    uint32_t UintAt(size_t pixel)const;
};
NumericImage LoadNumericDds(const std::filesystem::path&);
}
