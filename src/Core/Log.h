#pragma once
#include <filesystem>
#include <string_view>
namespace isr {
void OpenLog(const std::filesystem::path& path);
std::filesystem::path LogPath();
void Log(std::string_view text);
}
