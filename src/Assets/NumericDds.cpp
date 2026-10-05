#include "Assets/NumericDds.h"
#include <array>
#include <cstring>
#include <cmath>
#include <fstream>
#include <stdexcept>
namespace isr {
float NumericImage::FloatAt(size_t p,unsigned c)const{float v;std::memcpy(&v,bytes.data()+(p*Channels()+c)*4,4);return v;}
uint32_t NumericImage::UintAt(size_t p)const{uint32_t v;std::memcpy(&v,bytes.data()+p*4,4);return v;}
NumericImage LoadNumericDds(const std::filesystem::path& path){
    auto require=[](bool ok,const char* reason){if(!ok)throw std::runtime_error(std::string("Runtime DDS: ")+reason);};
    std::ifstream stream(path,std::ios::binary|std::ios::ate);require(bool(stream),"cannot open file");
    const auto length=stream.tellg();require(length>=148&&length<=148+2048ll*2048*16,"file size outside limits");
    stream.seekg(0);std::array<uint32_t,37> words{};require(bool(stream.read(reinterpret_cast<char*>(words.data()),148)),"truncated header");
    NumericImage image;image.height=words[3];image.width=words[4];image.format=static_cast<NumericFormat>(words[32]);
    require(image.width&&image.width<=2048&&image.height&&image.height<=2048,"dimensions outside 1..2048");
    require(image.format==NumericFormat::Vector||image.format==NumericFormat::Float||image.format==NumericFormat::Label,"unsupported DXGI format");
    std::array<uint32_t,37> expected{};expected[0]=0x20534444;expected[1]=124;expected[2]=0x100f;
    expected[3]=image.height;expected[4]=image.width;expected[5]=image.width*image.Channels()*4;expected[7]=1;
    expected[19]=32;expected[20]=4;expected[21]=0x30315844;expected[27]=0x1000;
    expected[32]=uint32_t(image.format);expected[33]=3;expected[35]=1;
    require(words==expected,"expected restricted DX10 2D single mip/slice uncompressed header");
    const size_t bytes=size_t(image.width)*image.height*image.Channels()*4;
    require(length==std::streamoff(148+bytes),"exact payload byte count mismatch");image.bytes.resize(bytes);
    require(bool(stream.read(reinterpret_cast<char*>(image.bytes.data()),bytes)),"truncated payload");
    if(image.format!=NumericFormat::Label)for(size_t i=0;i<bytes/4;++i){float v;std::memcpy(&v,image.bytes.data()+i*4,4);require(std::isfinite(v),"NaN/Inf in payload");}
    return image;
}
}
