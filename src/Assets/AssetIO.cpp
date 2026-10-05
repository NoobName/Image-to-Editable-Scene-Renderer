#include "Assets/AssetIO.h"
#include "Core/Error.h"
#include <wincrypt.h>
#include <fstream>
namespace isr {
std::string PathUtf8(const std::filesystem::path& path) { auto s=path.u8string(); return {s.begin(),s.end()}; }
std::vector<uint8_t> ReadAssetFile(const std::filesystem::path& path) {
    std::ifstream file(path,std::ios::binary|std::ios::ate);
    if (!file) throw std::runtime_error("Cannot open asset: " + PathUtf8(path));
    const auto size=file.tellg();
    if (size < 0 || size > 1024ll*1024*1024) throw std::runtime_error("Asset exceeds 1 GiB limit");
    std::vector<uint8_t> data(static_cast<size_t>(size)); file.seekg(0);
    if (!file.read(reinterpret_cast<char*>(data.data()),size)) throw std::runtime_error("Cannot read asset");
    return data;
}
std::filesystem::path ConstrainAssetPath(const std::filesystem::path& path,const std::filesystem::path& allowedRoot,bool requireFile) {
    if(allowedRoot.empty()) return path;
    std::error_code error;
    const auto root=std::filesystem::canonical(allowedRoot,error);
    if(error) throw std::runtime_error("Cannot resolve asset root: " + PathUtf8(allowedRoot));
    const auto resolved=std::filesystem::canonical(path,error);
    if(error) throw std::runtime_error("Cannot resolve asset: " + PathUtf8(path));
    auto p=resolved.begin();
    for(auto r=root.begin();r!=root.end();++r,++p)
        if(p==resolved.end() || _wcsicmp(p->c_str(),r->c_str())!=0) throw std::runtime_error("Asset escapes ScenePackage: " + PathUtf8(path));
    if(requireFile&&!std::filesystem::is_regular_file(resolved)) throw std::runtime_error("Asset is not a regular file: " + PathUtf8(path));
    return resolved;
}
std::vector<uint8_t> ReadImageUri(const std::filesystem::path& directory,const char* uri,const std::filesystem::path& allowedRoot) {
    if (!uri) throw std::runtime_error("Image has neither URI nor bufferView");
    const std::string value(uri);
    if (value.starts_with("data:")) {
        const auto comma=value.find(',');
        if (comma == std::string::npos || value.substr(0,comma).find(";base64") == std::string::npos)
            throw std::runtime_error("Only base64 data image URIs are supported");
        const auto encoded=value.substr(comma+1); DWORD size=0;
        CheckWin32(CryptStringToBinaryA(encoded.c_str(),static_cast<DWORD>(encoded.size()),CRYPT_STRING_BASE64|CRYPT_STRING_STRICT,nullptr,&size,nullptr,nullptr));
        std::vector<uint8_t> bytes(size);
        CheckWin32(CryptStringToBinaryA(encoded.c_str(),static_cast<DWORD>(encoded.size()),CRYPT_STRING_BASE64|CRYPT_STRING_STRICT,bytes.data(),&size,nullptr,nullptr));
        bytes.resize(size); return bytes;
    }
    if (value.find(":") != std::string::npos) throw std::runtime_error("Images must use relative local URIs");
    std::string decoded;
    for (size_t i=0;i<value.size();++i) {
        if (value[i] != '%') { decoded += value[i]; continue; }
        if (i+2 >= value.size()) throw std::runtime_error("Malformed percent-encoded URI");
        auto digit=[](char c)->int { if(c>='0'&&c<='9')return c-'0'; if(c>='A'&&c<='F')return c-'A'+10; if(c>='a'&&c<='f')return c-'a'+10; return -1; };
        int a=digit(value[i+1]),b=digit(value[i+2]);
        if (a<0||b<0||(a==0&&b==0)) throw std::runtime_error("Invalid URI escape");
        decoded += static_cast<char>(a*16+b); i+=2;
    }
    const auto relative=std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(decoded.data()),decoded.size()));
    if (relative.is_absolute()) throw std::runtime_error("Image URI must be relative");
    return ReadAssetFile(ConstrainAssetPath(directory/relative,allowedRoot));
}
}
