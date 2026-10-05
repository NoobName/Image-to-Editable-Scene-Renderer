#pragma once
#include <filesystem>
#include <span>
#include <vector>
#include <cstdint>
namespace isr {
std::vector<uint8_t> ReadAssetFile(const std::filesystem::path&);
std::vector<uint8_t> ReadImageUri(const std::filesystem::path& directory, const char* uri, const std::filesystem::path& allowedRoot = {});
std::filesystem::path ConstrainAssetPath(const std::filesystem::path& path, const std::filesystem::path& allowedRoot, bool requireFile = true);
std::string PathUtf8(const std::filesystem::path&);
}
