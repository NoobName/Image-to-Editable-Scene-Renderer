#include "ScenePackage/ImageFog.h"
#include <cstring>
#include <set>
namespace isr {
ImageFogData BuildImageFogData(const SourceObservation& source){
    ImageFogData out;out.distance={1,1,NumericFormat::Vector,std::vector<uint8_t>(16)};
    out.report={{"available",false},{"reason","Requires validated camera-Z/validity/region/geometry confidence and intrinsics"},
        {"sourceAdditionalDensity",0},{"distanceConvention","length(LH camera point); ray distance, NOT camera Z"},
        {"sampling","guarded bilinear within valid same-label 5% depth neighborhood; otherwise nearest"},
        {"policy","Invalid, zero label, known sky, unknown scale and distances >1e6 are protected; relative/synthetic requires explicit consent"}};
    if(!source.analysisMaps)return out;
    const auto& data=*source.analysisMaps;const auto& maps=data.maps;
    for(size_t i:{0,4,5,9})if(!maps[i].image)return out;
    if(!data.metadata.contains("camera"))return out;
    const auto& camera=data.metadata.at("camera");const std::string scale=camera.value("scaleType","unknown");
    out.report["scaleType"]=scale;out.relative=scale=="relative"||scale=="synthetic";
    if(scale!="metric"&&!out.relative){out.report["reason"]="Unknown geometry scale; fog unavailable";return out;}
    const auto& k=camera.at("intrinsicsNormalized");const double fx=k.at(0),fy=k.at(4),cx=k.at(2),cy=k.at(5);
    if(!std::isfinite(fx)||!std::isfinite(fy)||fx<=0||fy<=0)throw std::runtime_error("Fog requires finite positive intrinsics");
    const auto& z=*maps[0].image;const size_t count=size_t(z.width)*z.height;
    if(count>4ull*1024*1024)throw std::runtime_error("Fog analysis exceeds 4M pixels");
    for(size_t i:{3,4,5,9})if(maps[i].image&&(maps[i].image->width!=z.width||maps[i].image->height!=z.height))throw std::runtime_error("Fog map dimensions mismatch");
    out.distance={z.width,z.height,NumericFormat::Vector,std::vector<uint8_t>(count*16)};
    std::set<uint32_t> sky;for(const auto& region:source.regions)if(region.category=="sky")sky.insert(region.label);
    size_t supported=0,excludedSky=0;double maximum=0;
    for(size_t i=0;i<count;++i){
        const uint32_t label=maps[5].image->UintAt(i);excludedSky+=sky.contains(label);
        if(!maps[4].image->UintAt(i)||!label||sky.contains(label))continue;
        const double depth=z.FloatAt(i),confidence=maps[9].image->FloatAt(i);
        const double u=(double(i%z.width)+.5)/z.width,v=(double(i/z.width)+.5)/z.height;
        const double x=maps[3].image?maps[3].image->FloatAt(i,0):(u-cx)*depth/fx;
        const double y=maps[3].image?maps[3].image->FloatAt(i,1):-(v-cy)*depth/fy;
        const double distance=std::sqrt(x*x+y*y+depth*depth);
        if(!std::isfinite(distance)||distance<=0||distance>1e6||depth<=0||!std::isfinite(confidence)||confidence<=0)continue;
        const float values[]{float(distance),float(std::clamp(confidence,0.,1.)),float(depth),1};
        std::memcpy(out.distance.bytes.data()+i*16,values,16);++supported;maximum=std::max(maximum,distance);
    }
    out.available=supported>0;out.report["available"]=out.available;out.report["supportedPixels"]=supported;out.report["skyPixels"]=excludedSky;
    out.report["units"]=scale=="metric"?"meters":scale+"-units";out.report["densityUnits"]=scale=="metric"?"1/meters":"1/"+scale+"-units";
    out.report["positionSource"]=maps[3].image?"validated point map":"camera-Z times source-camera ray";
    out.report["geometryProvenance"]=maps[9].metadata;out.report["maxDistance"]=maximum;out.report["size"]={z.width,z.height};
    out.report["reason"]=out.available?(out.relative?"Relative/synthetic units: explicitly enable before adding fog":"Estimated metric distance; accuracy limited by geometry") : "No supported geometry pixels";
    return out;
}
}
