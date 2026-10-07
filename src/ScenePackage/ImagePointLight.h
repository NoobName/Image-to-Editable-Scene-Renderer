#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>
#include <optional>
namespace isr {
inline constexpr size_t MaxImagePointLights=4;
// Target-only lights in the immutable source camera's LH frame. Distances use
// the point map's units (not necessarily metres). No inferred source point light.
struct ImagePointLight {
    uint32_t id=1;
    std::array<float,3> position{0,0,1},color{1,1,1};
    float intensity=1,range=2;
    bool enabled=true;
    bool operator==(const ImagePointLight&)const=default;
    bool Valid()const{
        if(!id||id>=UINT32_MAX||!std::isfinite(intensity)||intensity<0||intensity>10000||!std::isfinite(range)||range<.01f||range>10000)return false;
        for(float x:position)if(!std::isfinite(x)||std::abs(x)>100000)return false;
        for(float x:color)if(!std::isfinite(x)||x<0||x>1)return false;
        return position[2]>=.01f;
    }
};
struct PointLightInteraction {
    bool placing=false,showHelpers=true;
    std::optional<size_t> selected,dragging;
    float dragDepth=1;
    void Cancel(){placing=false;dragging.reset();}
};
}
