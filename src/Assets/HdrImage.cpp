#include "Assets/HdrImage.h"
#include <fstream>
#include <sstream>
#include <array>
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace isr {
namespace {
using Rgbe=std::array<unsigned char,4>;
unsigned char Byte(std::istream& stream){char c;if(!stream.get(c))throw std::runtime_error("Truncated Radiance HDR");return static_cast<unsigned char>(c);}
Rgbe Pixel(std::istream& in){return {Byte(in),Byte(in),Byte(in),Byte(in)};}
DirectX::XMFLOAT4 Decode(Rgbe p){
    if(!p[3])return {0,0,0,1};const float scale=std::ldexp(1.0f,int(p[3])-136);
    DirectX::XMFLOAT4 out{p[0]*scale,p[1]*scale,p[2]*scale,1};
    if(!std::isfinite(out.x+out.y+out.z))throw std::runtime_error("HDR radiance overflow");return out;
}
Rgbe Encode(const DirectX::XMFLOAT4& p){
    if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z)||p.x<0||p.y<0||p.z<0)throw std::invalid_argument("HDR needs finite nonnegative RGB");
    float m=std::max({p.x,p.y,p.z});
    if(m<1e-32f)return {};int e;const float scale=std::frexp(m,&e)*256/m;if(e>127)throw std::invalid_argument("RGBE exponent overflow");
    return {static_cast<unsigned char>(p.x*scale),static_cast<unsigned char>(p.y*scale),static_cast<unsigned char>(p.z*scale),static_cast<unsigned char>(e+128)};
}
}
HdrImage LoadHdr(const std::filesystem::path& path){
    std::ifstream in(path,std::ios::binary);if(!in)throw std::runtime_error("Cannot open HDR environment");
    std::string line;std::getline(in,line);if(!line.empty()&&line.back()=='\r')line.pop_back();
    if(line!="#?RADIANCE"&&line!="#?RGBE")throw std::runtime_error("Expected Radiance .hdr RGBE environment (EXR is not supported)");
    bool format=false,ended=false;for(unsigned count=0;count<256&&std::getline(in,line);++count){
        if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.empty()){ended=true;break;}
        if(line=="FORMAT=32-bit_rle_rgbe")format=true;
    }
    if(!format||!ended||!std::getline(in,line))throw std::runtime_error("Invalid RGBE header/format");
    std::istringstream dimensions(line);std::string yaxis,xaxis;unsigned width=0,height=0;
    if(!(dimensions>>yaxis>>height>>xaxis>>width)||(yaxis!="-Y"&&yaxis!="+Y")||(xaxis!="+X"&&xaxis!="-X"))throw std::runtime_error("HDR requires Y-major +/-Y +/-X orientation");
    if(!width||!height||width>16384||height>8192||uint64_t(width)*height>32ull*1024*1024)throw std::runtime_error("HDR exceeds 32M pixel import limit");
    HdrImage image{width,height,{}};image.pixels.resize(size_t(width)*height);std::vector<Rgbe> scanline(width);
    for(unsigned row=0;row<height;++row){Rgbe first=Pixel(in);
        if(width>=8&&width<=32767&&first[0]==2&&first[1]==2&&(first[2]&128)==0){
            if((unsigned(first[2])*256+first[3])!=width)throw std::runtime_error("HDR RLE scanline width mismatch");
            for(unsigned channel=0;channel<4;++channel){unsigned x=0;while(x<width){unsigned code=Byte(in),count=code>128?code-128:code;
                if(!count||count>width-x)throw std::runtime_error("Invalid HDR RLE packet");
                if(code>128){auto v=Byte(in);for(unsigned i=0;i<count;++i)scanline[x++][channel]=v;}
                else for(unsigned i=0;i<count;++i)scanline[x++][channel]=Byte(in);
            }}
        }else{
            scanline[0]=first;for(unsigned x=1;x<width;++x)scanline[x]=Pixel(in);
            for(auto p:scanline)if(p[0]==1&&p[1]==1&&p[2]==1)throw std::runtime_error("Legacy RGBE repeat encoding is unsupported; re-save as modern RLE HDR");
        }
        for(unsigned x=0;x<width;++x){unsigned dstY=yaxis=="-Y"?row:height-row-1,dstX=xaxis=="+X"?x:width-x-1;image.pixels[size_t(dstY)*width+dstX]=Decode(scanline[x]);}
    }
    return image;
}
void SaveHdr(const std::filesystem::path& path,const HdrImage& image){
    if(!image.width||!image.height||image.pixels.size()!=size_t(image.width)*image.height)throw std::invalid_argument("Invalid HDR image dimensions");
    if(path.has_parent_path())std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path,std::ios::binary);out.exceptions(std::ios::badbit|std::ios::failbit);
    out<<"#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y "<<image.height<<" +X "<<image.width<<'\n';
    std::vector<Rgbe> row(image.width);
    for(unsigned y=0;y<image.height;++y){for(unsigned x=0;x<image.width;++x)row[x]=Encode(image.pixels[size_t(y)*image.width+x]);
        if(image.width<8||image.width>32767){for(const auto& p:row)out.write(reinterpret_cast<const char*>(p.data()),4);continue;}
        const Rgbe header{2,2,static_cast<unsigned char>(image.width>>8),static_cast<unsigned char>(image.width&255)};out.write(reinterpret_cast<const char*>(header.data()),4);
        for(unsigned c=0;c<4;++c)for(unsigned x=0;x<image.width;){unsigned count=std::min(128u,image.width-x);out.put(static_cast<char>(count));
            for(unsigned i=0;i<count;++i)out.put(static_cast<char>(row[x++][c]));}
    }
}
std::vector<HdrImage> HdrMipChain(HdrImage source){
    if(!source.width||!source.height||source.pixels.size()!=size_t(source.width)*source.height)throw std::invalid_argument("Invalid HDR mip source dimensions");
    for(const auto& p:source.pixels)if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z)||p.x<0||p.y<0||p.z<0)throw std::invalid_argument("HDR needs finite nonnegative RGB");
    std::vector<HdrImage> mips;mips.push_back(std::move(source));
    while(mips.back().width>1||mips.back().height>1){const auto& previous=mips.back();HdrImage next{std::max(1u,previous.width/2),std::max(1u,previous.height/2),{}};next.pixels.resize(size_t(next.width)*next.height);
        for(unsigned y=0;y<next.height;++y)for(unsigned x=0;x<next.width;++x){float sum[3]{},weight=0;
            float x0=float(x)*previous.width/next.width,x1=float(x+1)*previous.width/next.width,y0=float(y)*previous.height/next.height,y1=float(y+1)*previous.height/next.height;
            for(unsigned sy=unsigned(y0);sy<std::min(previous.height,unsigned(std::ceil(y1)));++sy)for(unsigned sx=unsigned(x0);sx<std::min(previous.width,unsigned(std::ceil(x1)));++sx){
                float w=(std::min(x1,float(sx+1))-std::max(x0,float(sx)))*(std::min(y1,float(sy+1))-std::max(y0,float(sy)));const auto& p=previous.pixels[size_t(sy)*previous.width+sx];
                sum[0]+=p.x*w;sum[1]+=p.y*w;sum[2]+=p.z*w;weight+=w;}
            next.pixels[size_t(y)*next.width+x]={sum[0]/weight,sum[1]/weight,sum[2]/weight,1};
        }mips.push_back(std::move(next));
    }return mips;
}
}
