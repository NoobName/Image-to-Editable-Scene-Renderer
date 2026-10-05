#include "Assets/TextureProcessing.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace isr {
float SrgbToLinear(float x) { return x<=0.04045f?x/12.92f:std::pow((x+0.055f)/1.055f,2.4f); }
float LinearToSrgb(float x) { return x<=0.0031308f?x*12.92f:1.055f*std::pow(x,1.0f/2.4f)-0.055f; }
std::vector<ImageData> BuildMipChain(const ImageData& image,bool srgb) {
    if(!image.width||!image.height||image.rgba.size()!=size_t(image.width)*image.height*4)throw std::runtime_error("Invalid image pixels");
    std::vector<ImageData> chain{image};
    while(chain.back().width>1||chain.back().height>1) {
        const auto& source=chain.back(); ImageData next; next.width=std::max(1u,source.width/2);next.height=std::max(1u,source.height/2);
        next.rgba.resize(size_t(next.width)*next.height*4);
        for(uint32_t y=0;y<next.height;++y)for(uint32_t x=0;x<next.width;++x) {
            // Area-weighted box filtering includes every texel for odd dimensions.
            const float x0=float(x)*source.width/next.width,x1=float(x+1)*source.width/next.width;
            const float y0=float(y)*source.height/next.height,y1=float(y+1)*source.height/next.height;
            for(unsigned c=0;c<4;++c) {
                float sum=0,weight=0;
                for(uint32_t sy=uint32_t(y0);sy<std::min(source.height,uint32_t(std::ceil(y1)));++sy)
                    for(uint32_t sx=uint32_t(x0);sx<std::min(source.width,uint32_t(std::ceil(x1)));++sx) {
                        const float w=(std::min(x1,float(sx+1))-std::max(x0,float(sx)))*(std::min(y1,float(sy+1))-std::max(y0,float(sy)));
                        float v=source.rgba[(size_t(sy)*source.width+sx)*4+c]/255.0f;
                        if(srgb&&c<3)v=SrgbToLinear(v);sum+=v*w;weight+=w;
                    }
                float v=sum/weight;if(srgb&&c<3)v=LinearToSrgb(v);
                next.rgba[(size_t(y)*next.width+x)*4+c]=static_cast<uint8_t>(std::lround(std::clamp(v,0.0f,1.0f)*255));
            }
        }
        chain.push_back(std::move(next));
    }
    return chain;
}
}
