#include "ScenePackage/ImageFog.h"
#include <cstring>
#include <iostream>
using namespace isr;
static void Need(bool b,const char* s){if(!b)throw std::runtime_error(s);}
int main(){try{
    SourceObservation source;Need(!BuildImageFogData(source).available,"Absent maps must not invent distance");
    auto maps=std::make_shared<AnalysisMaps>();source.analysisMaps=maps;
    maps->metadata={{"camera",{{"scaleType","metric"},{"intrinsicsNormalized",{1,0,.5,0,1,.5,0,0,1}}}}};
    for(size_t m:{0,4,5,9}){NumericImage image{5,3,m==4||m==5?NumericFormat::Label:NumericFormat::Float,{}};image.bytes.resize(60);
        for(size_t i=0;i<15;++i){float f=m==0?2.f:1.f;uint32_t v=1;std::memcpy(image.bytes.data()+i*4,m==4||m==5?static_cast<void*>(&v):static_cast<void*>(&f),4);}maps->maps[m].image=image;}
    auto data=BuildImageFogData(source);Need(data.available&&!data.relative,"Metric support");
    Need(data.distance.FloatAt(7)==2&&data.distance.FloatAt(0)>2,"Camera Z is not ray distance");
    Need(std::abs(FogTransmission(.5f,2)-std::exp(-1.f))<1e-7f,"Exponential attenuation");
    Need(FogTransmission(0,2)==1&&std::isfinite(FogTransmission(1000,1e6f)),"Zero/extreme must remain finite");
    maps->metadata["camera"]["scaleType"]="relative";Need(BuildImageFogData(source).relative,"Relative distance must keep units");
    maps->metadata["camera"]["scaleType"]="unknown";Need(!BuildImageFogData(source).available,"Unknown scale protected");
    maps->metadata["camera"]["scaleType"]="metric";uint32_t zero=0;std::memcpy(maps->maps[4].image->bytes.data()+28,&zero,4);
    Need(BuildImageFogData(source).distance.FloatAt(7,3)==0,"Unknown depth protected");
    source.regions.push_back({1,"sky-01","Sky","sky"});Need(!BuildImageFogData(source).available,"Known sky protected");
    ImageFogParameters p;Need(p.Valid()&&!p.enabled,"Off default");p.density=NAN;Need(!p.Valid(),"NaN rejected");p.density=1001;Need(!p.Valid(),"Bounded density");
    p.density=1;p.airlight[0]=INFINITY;Need(!p.Valid(),"Nonfinite airlight rejected");
    std::cout<<"Fog CPU: ray distance, metric/relative/unknown, sky/invalid, zero/extreme and parameter bounds passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
