#pragma once
#include <filesystem>
#include "Scene/LookParameters.h"
namespace isr {
enum class RenderMode { Final, Albedo, Normal, Roughness, Metallic, Depth, Wireframe, OriginalImage, EstimatedAlbedo, EstimatedNormal, EstimatedRoughness };
inline constexpr int RenderModeCount=11;
inline constexpr const char* RenderModeNames[]={"最终效果","反照率","法线","粗糙度","金属度","深度","线框",
    "原图（投影在几何上）","估计反照率","估计法线","估计粗糙度"};
static_assert(sizeof(RenderModeNames)/sizeof(RenderModeNames[0])==RenderModeCount);
struct RenderSettings {
    RenderMode mode=RenderMode::Final;
    LookParameters look;
    float ambient=0; // Optional legacy fill; real environment lighting is independent.
    bool shadows=true;
    int shadowPcfRadius=1; // 0: hard shadow, 1: 3x3 PCF, 2: 5x5 PCF.
    float shadowBias=0.0005f,shadowNormalBias=0.02f;
    bool ibl=true,skybox=true;
    float environmentIntensity=1,environmentRotation=0; // radians, about world +Y
    std::filesystem::path environmentPath;
};
}
