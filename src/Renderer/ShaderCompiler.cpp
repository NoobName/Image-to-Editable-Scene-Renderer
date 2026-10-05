#include "Renderer/ShaderCompiler.h"
#include "Core/Log.h"
#include <vector>
namespace isr {
std::filesystem::path ExecutableDirectory() {
    wchar_t path[32768]{}; const DWORD size = GetModuleFileNameW(nullptr, path, 32768);
    if (!size || size == 32768) throw std::runtime_error("Cannot resolve executable directory");
    return std::filesystem::path(path).parent_path();
}
ShaderCompiler::ShaderCompiler() {
    module_.handle = LoadLibraryExW((ExecutableDirectory() / "dxcompiler.dll").c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if (!module_.handle) Check(HRESULT_FROM_WIN32(GetLastError()));
    const auto create = reinterpret_cast<DxcCreateInstanceProc>(GetProcAddress(module_.handle, "DxcCreateInstance"));
    if (!create) throw std::runtime_error("Missing DxcCreateInstance");
    Check(create(CLSID_DxcUtils, IID_PPV_ARGS(&utils_)));
    Check(create(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler_)));
    Check(utils_->CreateDefaultIncludeHandler(&includes_));
}
ComPtr<IDxcBlob> ShaderCompiler::Compile(const std::filesystem::path& path, const wchar_t* entry, ShaderStage stage, bool debug) const {
    ComPtr<IDxcBlobEncoding> source; Check(utils_->LoadFile(path.c_str(), nullptr, &source));
    DxcBuffer buffer{source->GetBufferPointer(), source->GetBufferSize(), DXC_CP_UTF8};
    const wchar_t* profile = stage == ShaderStage::Vertex ? L"vs_6_0" : stage == ShaderStage::Pixel ? L"ps_6_0" : L"cs_6_0";
    std::vector<LPCWSTR> args{path.c_str(), L"-E", entry, L"-T", profile, L"-HV", L"2021", L"-WX"};
    if (debug) args.insert(args.end(), {L"-Zi", L"-Qembed_debug", L"-Od"}); else args.push_back(L"-O3");
    ComPtr<IDxcResult> result;
    Check(compiler_->Compile(&buffer, args.data(), static_cast<UINT32>(args.size()), includes_.Get(), IID_PPV_ARGS(&result)));
    ComPtr<IDxcBlobUtf8> errors; Check(result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr));
    if (errors && errors->GetStringLength()) Log(errors->GetStringPointer());
    HRESULT status{}; Check(result->GetStatus(&status)); Check(status);
    ComPtr<IDxcBlob> object; Check(result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&object), nullptr));
    Log("DXC compiled " + path.filename().string() + (debug ? " (debug)" : " (optimized)")); return object;
}
}
