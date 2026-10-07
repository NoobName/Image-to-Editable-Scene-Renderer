#pragma once
#include "ScenePackage/SourceObservation.h"
#include <cmath>
#include <algorithm>
#include <cstring>
#include <optional>
namespace isr {
// Display coordinates are normalized source-edge coordinates. They never change camera intrinsics.
struct ImageDisplayState {
    float zoom=1,panX=0,panY=0,exposure=0,wipe=.5f,confidenceThreshold=.35f;
    int comparison=0; // Single / linked side-by-side / wipe.
    bool lowConfidence=false;
    void Fit(){zoom=1;panX=panY=0;}
};
struct RegionProtectionEdit {
    std::optional<uint32_t> selected;
    std::map<uint32_t,float> weights;
    uint64_t revision=0;
    float draft=1;
    void Apply(uint32_t label,float weight){weights[label]=std::clamp(weight,0.f,1.f);++revision;}
    void Clear(){weights.clear();++revision;}
};
inline std::optional<uint32_t> SourceLabel(const SourceObservation& source,float u,float v){
    if(u<0||v<0||u>=1||v>=1||!source.analysisMaps)return {};
    const auto& map=source.analysisMaps->maps[5].image;if(!map)return {};
    const auto x=std::min(uint32_t(u*map->width),map->width-1),y=std::min(uint32_t(v*map->height),map->height-1);
    uint32_t label;std::memcpy(&label,map->bytes.data()+(size_t(y)*map->width+x)*4,4);return label;
}
}
