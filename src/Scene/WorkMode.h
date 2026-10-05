#pragma once
namespace isr {
// Workspace intent is independent of the existing 3D RenderMode shader values.
enum class WorkMode { Scene3D, ImageRelighting };
enum class ImageDebugView { Original, PixelGrid, Depth, GeometryNormal, WorldNormal, Position, Validity, Region,
    Albedo, Roughness, Metallic, GeometryConfidence, MaterialConfidence, RegionConfidence, TangentNormal,
    ShadingProxy, OldShading, LightingResidual, FitMask };
inline constexpr const char* ImageDebugKeys[]={"original","grid","depth","geometry-normal","world-normal","position","validity","region",
    "albedo","roughness","metallic","geometry-confidence","material-confidence","region-confidence","tangent-normal",
    "shading-proxy","old-shading","lighting-residual","fit-mask"};
inline constexpr const char* ImageDebugNames[]={"Source / Original View","Source / Pixel Grid","Depth (camera Z)","Geometry Normal (camera LH)","Geometry Normal (world LH)",
    "Position (camera LH)","Validity","Region IDs","Estimated Albedo (linear)","Roughness","Metallic","Geometry confidence","Material confidence","Region confidence","Material tangent normal",
    "Original Shading Proxy /4","Fitted Old Shading /4","Lighting Residual Y [0,1]","Lighting Fit Mask"};
inline constexpr int ImageDebugCount=19;
}
