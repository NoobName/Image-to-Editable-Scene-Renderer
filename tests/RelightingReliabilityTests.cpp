#include "ScenePackage/RelightingReliability.h"
#include <cstring>
#include <iostream>
using namespace isr;
namespace {void Require(bool b,const char* m){if(!b)throw std::runtime_error(m);}}
int main(){try{
    AnalysisMaps maps;constexpr uint32_t w=7,h=3;
    for(size_t i:{0,1,4,5,7,8,9,10,11}){NumericImage im;im.width=w;im.height=h;im.format=i==1?NumericFormat::Vector:(i==4||i==5)?NumericFormat::Label:NumericFormat::Float;
        im.bytes.resize(w*h*im.Channels()*4);maps.maps[i].image=std::move(im);maps.maps[i].metadata={{"provenance",{{"kind","estimated"},{"meaning","test quality"}}}};}
    maps.maps[9].metadata["provenance"]={{"kind","synthetic"},{"meaning","synthetic-validity"}};
    auto put=[&](size_t i,size_t p,float v,unsigned c=0){std::memcpy(maps.maps[i].image->bytes.data()+(p*maps.maps[i].image->Channels()+c)*4,&v,4);};
    auto label=[&](size_t i,size_t p,uint32_t v){std::memcpy(maps.maps[i].image->bytes.data()+p*4,&v,4);};
    for(size_t p=0;p<w*h;++p){put(0,p,p%w<3?3.f:7.f);put(1,p,-1,2);label(4,p,1);label(5,p,p%w<3?16777217u:4294967295u);
        put(7,p,.8f);put(8,p,0);put(9,p,1);put(10,p,1);put(11,p,1);}
    auto r=BuildRelightingReliability(&maps,nullptr,w,h);
    Require(r.weights[0].FloatAt(w+1,0)==1&&r.weights[0].FloatAt(w+1,2)==1,"Trusted geometric/material evidence weakened");
    Require(r.weights[0].FloatAt(w+2,1)==.35f&&r.weights[0].FloatAt(w+3,1)==.35f,"Depth/uint-label boundary not detected");
    Require(r.weights[0].FloatAt(w+1,3)==.5f,"Missing fit must use conservative declared default");
    put(8,w+1,.95f);label(4,w+5,0);r=BuildRelightingReliability(&maps,nullptr,w,h);
    Require(r.weights[0].FloatAt(w+1,2)==0&&r.weights[0].FloatAt(w+5,0)==0,"Reflection/invalid protection failed");
    maps.maps[9].metadata["provenance"]={{"kind","estimated"},{"meaning","binary-validity-not-calibrated"}};
    maps.maps[10].metadata["provenance"]={{"kind","fallback"},{"meaning","zero no material inference"}};
    put(8,w+1,0);put(10,w+1,0);r=BuildRelightingReliability(&maps,nullptr,w,h);
    Require(r.weights[0].FloatAt(w+1,0)==.8f&&r.weights[0].FloatAt(w+1,2)==.75f,"Binary validity/neutral material provenance confused with probabilities");
    std::cout<<"Reliability provenance, tangent independence, defaults, UINT label/depth boundaries, invalid and reflection protection: OK\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
