#include "Assets/HdrImage.h"
#include <cmath>
#include <iostream>
using namespace isr;
int wmain(int argc,wchar_t** argv){try{
    if(argc!=2)throw std::runtime_error("Usage: GenerateEnvironments <directory>");
    const std::filesystem::path directory=argv[1];
    for(unsigned variant=0;variant<2;++variant){HdrImage image{512,256,{}};image.pixels.resize(size_t(image.width)*image.height);
        for(unsigned y=0;y<image.height;++y)for(unsigned x=0;x<image.width;++x){
            float u=(float(x)+0.5f)/image.width,v=(float(y)+0.5f)/image.height;
            float phi=(u-0.5f)*DirectX::XM_2PI,theta=v*DirectX::XM_PI;
            float dx=std::sin(theta)*std::cos(phi),dy=std::cos(theta),dz=std::sin(theta)*std::sin(phi);
            DirectX::XMFLOAT4 color{};
            if(variant==0){
                float base=dy>0?0.18f:0.045f;color={base*0.7f,base*0.85f,base,1};
                bool panel=dy>0.05f&&dy<0.75f&&((dx>0.78f&&std::abs(dz)<0.45f)||(dz< -0.82f&&dx<0.25f&&dx> -0.5f));
                if(panel)color={9.0f,10.0f,12.0f,1};
                if(dx< -0.92f&&dy> -0.1f&&dy<0.45f)color={6,0.5f,0.15f,1};
            }else{
                float horizon=std::exp(-std::abs(dy)*7);color={0.15f+1.6f*horizon,0.22f+0.55f*horizon,0.5f+0.1f*horizon,1};
                if(dy<0)color={0.08f,0.05f,0.025f,1};
                float sun=std::max(0.0f,dx* -0.35f+dy*0.35f+dz* -0.868907f);sun=std::pow(sun,350)*45;
                color.x+=sun;color.y+=sun*0.55f;color.z+=sun*0.16f;
                if(std::abs(dx)<0.35f&&dz>0.7f&&dy<0.5f&&dy>0)color={0.04f,0.12f,0.035f,1};
            }image.pixels[size_t(y)*image.width+x]=color;
        }SaveHdr(directory/(variant?"SunsetCourtyard.hdr":"SoftStudio.hdr"),image);
    }
    std::cout<<"Generated two original linear HDR environment fixtures\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
