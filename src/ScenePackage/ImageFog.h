#pragma once
#include "ScenePackage/SourceObservation.h"
#include <algorithm>
#include <cmath>
namespace isr {
// Additional atmosphere only: the observed image already contains its unknown original atmosphere.
// Density is inverse geometry units, never a recovered physical extinction coefficient.
struct ImageFogParameters {
    bool enabled=false,allowRelativeScale=false;
    float density=0;
    std::array<float,3> airlight{.7f,.8f,1.f}; // Linear sRGB, before the single display exposure.
    bool Valid()const{return std::isfinite(density)&&density>=0&&density<=1000&&
        std::all_of(airlight.begin(),airlight.end(),[](float x){return std::isfinite(x)&&x>=0&&x<=1;});}
    bool operator==(const ImageFogParameters&)const=default;
};
struct ImageFogData {
    NumericImage distance; // RGBA = ray distance, geometry confidence, camera Z, hard validity.
    package::Json report;
    bool available=false,relative=false;
};
ImageFogData BuildImageFogData(const SourceObservation&);
inline float FogTransmission(float density,float distance){return std::exp(-std::min(density*distance,80.f));}
}
