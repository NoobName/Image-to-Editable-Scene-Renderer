#pragma once
#include <cmath>
namespace isr {
enum class RatioColorMode { Luminance, BoundedColor };
struct RelightingParameters {
    float strength=1,epsilon=.02f,minRatio=.25f,maxRatio=4;
    RatioColorMode colorMode=RatioColorMode::BoundedColor;
    bool stability=true;
    bool intrinsicProtection=true;
    bool specularEnabled=true;
    bool castShadows=true;
    float shadowStrength=.65f,shadowConfidence=1,shadowMaxDelta=2,shadowBias=.0005f,shadowNormalBias=.005f;
    int shadowPcfRadius=1;
    float specularStrength=.25f,specularMaxDelta=.15f,specularRoughnessScale=1;
    float chromaLimit=1.5f,relativeDepthEdge=.1f,normalCosineEdge=.85f;
    bool operator==(const RelightingParameters&)const=default;
    bool Valid()const{return std::isfinite(strength)&&strength>=0&&strength<=1&&std::isfinite(epsilon)&&epsilon>=1e-6f&&epsilon<=1
        &&std::isfinite(minRatio)&&minRatio>=.01f&&minRatio<=1&&std::isfinite(maxRatio)&&maxRatio>=1&&maxRatio<=16
        &&std::isfinite(chromaLimit)&&chromaLimit>=1&&chromaLimit<=2&&std::isfinite(relativeDepthEdge)&&relativeDepthEdge>0&&relativeDepthEdge<=.5f
        &&std::isfinite(normalCosineEdge)&&normalCosineEdge>=0&&normalCosineEdge<=1
        &&std::isfinite(specularStrength)&&specularStrength>=0&&specularStrength<=1
        &&std::isfinite(specularMaxDelta)&&specularMaxDelta>=0&&specularMaxDelta<=.5f
        &&std::isfinite(specularRoughnessScale)&&specularRoughnessScale>=.5f&&specularRoughnessScale<=2
        &&std::isfinite(shadowStrength)&&shadowStrength>=0&&shadowStrength<=1&&std::isfinite(shadowConfidence)&&shadowConfidence>=0&&shadowConfidence<=1
        &&std::isfinite(shadowMaxDelta)&&shadowMaxDelta>=0&&shadowMaxDelta<=2&&std::isfinite(shadowBias)&&shadowBias>=0&&shadowBias<=.01f
        &&std::isfinite(shadowNormalBias)&&shadowNormalBias>=0&&shadowNormalBias<=.1f&&shadowPcfRadius>=0&&shadowPcfRadius<=2
        &&(colorMode==RatioColorMode::Luminance||colorMode==RatioColorMode::BoundedColor);}
    void Conservative(){stability=true;strength=1;epsilon=.03f;minRatio=.5f;maxRatio=2;chromaLimit=1.25f;relativeDepthEdge=.08f;normalCosineEdge=.9f;}
};
}
