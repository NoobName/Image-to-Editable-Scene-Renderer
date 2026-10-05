#include "ScenePackage/AnchorImage.h"
#include "ScenePackage/PackageManifest.h"
#include "Assets/AssetIO.h"
#include "Assets/ImageDecoder.h"
#include <windows.h>
#include <bcrypt.h>
#include <span>
#include <array>
#include <algorithm>
namespace isr::package {
namespace {
void Require(bool value,const std::string& reason){if(!value)throw std::runtime_error("AppearanceAnchor: "+reason);}
void CheckCrypto(NTSTATUS status){Require(status>=0,"Windows SHA-256 operation failed");}
}
std::string Sha256(std::span<const uint8_t> bytes){
    struct Algorithm {BCRYPT_ALG_HANDLE handle{};~Algorithm(){if(handle)BCryptCloseAlgorithmProvider(handle,0);}} algorithm;
    CheckCrypto(BCryptOpenAlgorithmProvider(&algorithm.handle,BCRYPT_SHA256_ALGORITHM,nullptr,0));
    struct Hash {BCRYPT_HASH_HANDLE handle{};~Hash(){if(handle)BCryptDestroyHash(handle);}} hash;
    CheckCrypto(BCryptCreateHash(algorithm.handle,&hash.handle,nullptr,0,nullptr,0,0));
    for(size_t offset=0;offset<bytes.size();offset+=65536){
        const auto size=static_cast<ULONG>(std::min(size_t(65536),bytes.size()-offset));
        CheckCrypto(BCryptHashData(hash.handle,const_cast<PUCHAR>(bytes.data()+offset),size,0));}
    std::array<unsigned char,32> digest{};CheckCrypto(BCryptFinishHash(hash.handle,digest.data(),32,0));
    constexpr char hex[]="0123456789abcdef";std::string result;result.reserve(64);
    for(auto b:digest){result+=hex[b>>4];result+=hex[b&15];}return result;
}
namespace {
uint32_t Big32(const uint8_t* p){return uint32_t(p[0])<<24|uint32_t(p[1])<<16|uint32_t(p[2])<<8|p[3];}
}
std::filesystem::path ValidateAnchorImage(const std::filesystem::path& root,const Json& record,bool original,std::shared_ptr<const ImageData>* retainPixels){
    const auto size=record[original?"storedSize":"size"].get<std::array<uint32_t,2>>();
    Require(uint64_t(size[0])*size[1]<=40000000,"image exceeds 40 megapixels");
    const auto digest=record["sha256"].get<std::string>();
    Require(digest.size()==64&&std::all_of(digest.begin(),digest.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');}),"invalid lowercase SHA-256");
    const auto path=AssetPath(root,record["path"],original?"relighting":"textures",original?std::vector<std::string>{".png",".jpg",".jpeg"}:std::vector<std::string>{".png"});
    Require(std::filesystem::file_size(path)<=128ull*1024*1024,"image exceeds 128 MiB");
    const auto bytes=ReadAssetFile(path);
    Require(bytes.size()<=128ull*1024*1024,"image exceeds 128 MiB");
    // Hash precisely the bytes decoded below, even if the on-disk file changes during loading.
    Require(Sha256(bytes)==digest,"SHA-256 mismatch: "+record["path"].get<std::string>());
    constexpr std::array<uint8_t,8> png={137,80,78,71,13,10,26,10};
    const bool isPng=bytes.size()>33&&std::equal(png.begin(),png.end(),bytes.begin());
    Require(isPng||(original&&bytes.size()>2&&bytes[0]==255&&bytes[1]==216),"unsupported image format");
    if(!original){
        Require(isPng&&Big32(bytes.data()+8)==13&&bytes[24]==8&&bytes[25]==2,"anchor/analysis must be RGB8 PNG");
    }
    if(isPng){
        for(size_t offset=8;offset+12<=bytes.size();){
            const auto length=Big32(bytes.data()+offset);Require(length<=bytes.size()-offset-12,"invalid PNG chunk");
            const std::string_view kind(reinterpret_cast<const char*>(bytes.data()+offset+4),4);
            Require(kind!="acTL","animated source images are unsupported");
            Require(original||(kind!="iCCP"&&kind!="eXIf"),"normalized PNG must not contain ICC/EXIF");
            offset+=size_t(length)+12;
        }
    }
    // Retain the source decode when requested: later GPU upload reads this validated snapshot,
    // not a second disk read that could silently pick up a different file.
    const auto decoded=DecodeImage(bytes,PathUtf8(path),40000000);
    Require(decoded->width==size[0]&&decoded->height==size[1],"image dimensions mismatch: "+record["path"].get<std::string>());
    if(retainPixels)*retainPixels=decoded;
    return path;
}
}
