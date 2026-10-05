#include "Core/Log.h"
#include <windows.h>
#include <fstream>
#include <mutex>
#include <stdexcept>
#include <string>
namespace isr {
namespace { std::ofstream output; std::mutex logMutex; std::filesystem::path outputPath; }
void OpenLog(const std::filesystem::path& path) {
    if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path());
    output.open(path);
    if (!output) throw std::runtime_error("Cannot open log file");
    outputPath=std::filesystem::absolute(path);
}
std::filesystem::path LogPath(){return outputPath;}
void Log(std::string_view text) {
    std::lock_guard lock(logMutex);
    const std::string line = std::string(text) + '\n';
    OutputDebugStringA(line.c_str());
    if (output) { output << line; output.flush(); }
}
}
