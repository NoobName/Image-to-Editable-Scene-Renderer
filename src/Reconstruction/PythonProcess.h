#pragma once
#include <filesystem>
#include <functional>
#include <stop_token>
#include <string>
#include <vector>
namespace isr {
std::wstring QuoteProcessArgument(const std::wstring&);
// Runs only on a worker. No shell, console window, Python embedding, or pipe back-pressure.
unsigned RunPythonProcess(const std::filesystem::path& python,const std::vector<std::wstring>& arguments,
    const std::filesystem::path& directory,const std::filesystem::path& log,std::stop_token,
    const std::function<void()>& poll);
}
