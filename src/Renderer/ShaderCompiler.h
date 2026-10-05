#pragma once
#include "Renderer/DeviceContext.h"
#include <dxcapi.h>
#include <filesystem>
namespace isr {
enum class ShaderStage { Vertex, Pixel, Compute };
class ShaderCompiler {
public:
    ShaderCompiler();
    ShaderCompiler(const ShaderCompiler&) = delete;
    ShaderCompiler& operator=(const ShaderCompiler&) = delete;
    ComPtr<IDxcBlob> Compile(const std::filesystem::path&, const wchar_t* entry, ShaderStage, bool debug) const;
private:
    struct Module { HMODULE handle{}; ~Module() { if (handle) FreeLibrary(handle); } } module_;
    ComPtr<IDxcUtils> utils_;
    ComPtr<IDxcCompiler3> compiler_;
    ComPtr<IDxcIncludeHandler> includes_;
};
std::filesystem::path ExecutableDirectory();
}
