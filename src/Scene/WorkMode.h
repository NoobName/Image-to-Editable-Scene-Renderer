#pragma once
namespace isr {
// Workspace intent is independent of the existing 3D RenderMode shader values.
enum class WorkMode { Scene3D, ImageRelighting };
enum class ImageDebugView { Original, PixelGrid, Depth, GeometryNormal, WorldNormal, Position, Validity, Region,
    Albedo, Roughness, Metallic, GeometryConfidence, MaterialConfidence, RegionConfidence, TangentNormal,
    ShadingProxy, OldShading, LightingResidual, FitMask, CalculatedOld, CalculatedNew, ShadingDifference, NormalLightDot, ShadingValidity,
    RelightingRatio, Relighted, RelitDifference, GeometryWeight, BoundaryWeight, MaterialWeight, FitWeight, SignalWeight, ShadowRiskWeight,
    RelightingConfidence, RawLogRatio, EffectiveLogRatio, ClampMask, Protection,
    IntrinsicAlbedo, IntrinsicShading, IntrinsicResidual, IntrinsicUncertainty, IntrinsicError, IntrinsicValidity, DiffuseSupport, ProtectedResidual,
    SpecularCandidate,SpecularConfidence,DiffuseAnchor,OldSpecular,NewSpecular,SpecularDelta,SpecularClip,SpecularDifference,SpecularProtected,ShadowCandidate,ShadowVisibility,ShadowGeometry,ShadowConfidence,ShadowUnknown,ShadowManualConfirm,ShadowManualProtect,ShadowEffective,ShadowOverlay,CastOldMap,CastNewMap,CastOldVisibility,CastNewVisibility,CastOldEstimate,CastConfidence,CastChange,CastDifference,CastFinal,CastBaseline,FogDistance,FogTransmission,FogConfidence,FogAirlight };
inline constexpr const char* ImageDebugKeys[]={"original","grid","depth","geometry-normal","world-normal","position","validity","region",
    "albedo","roughness","metallic","geometry-confidence","material-confidence","region-confidence","tangent-normal",
    "shading-proxy","old-shading","lighting-residual","fit-mask","calculated-old","calculated-new","shading-difference","normal-light-dot","shading-validity","ratio","relighted","relit-difference",
    "geometry-weight","boundary-weight","material-weight","fit-weight","signal-weight","shadow-risk-weight","relighting-confidence","raw-log-ratio","effective-log-ratio","clamp-mask","protection",
    "intrinsic-albedo","intrinsic-shading","intrinsic-residual","intrinsic-uncertainty","intrinsic-error","intrinsic-validity","diffuse-support","protected-residual",
    "specular-candidate","specular-confidence","diffuse-anchor","old-specular","new-specular","specular-delta","specular-clip","specular-difference","specular-protected",
    "shadow-candidate","shadow-visibility","shadow-geometry","shadow-confidence","shadow-unknown","shadow-manual-confirm","shadow-manual-protect","shadow-effective","shadow-overlay","cast-old-map","cast-new-map","cast-old-visibility","cast-new-visibility","cast-old-estimate","cast-confidence","cast-change","cast-difference","cast-final","cast-baseline","fog-distance","fog-transmittance","fog-confidence","fog-airlight"};
inline constexpr const char* ImageDebugNames[]={"Source / Original View","Source / Pixel Grid","Depth (camera Z)","Geometry Normal (camera LH)","Geometry Normal (world LH)",
    "Position (camera LH)","Validity","Region IDs","Estimated Albedo (linear)","Roughness","Metallic","Geometry confidence","Material confidence","Region confidence","Material tangent normal",
    "Estimated Original Shading (proxy /4)","Fitted Old Shading /4","Lighting Residual Y [0,1]","Lighting Fit Mask",
    "Calculated Old Shading /4","Calculated New Shading /4","Shading Difference abs /4","Normal / TO-light dot (negative=red)","Shading Validity",
    "Relighting Ratio (1 = gray)","Relighted Original RGB","Relighted Difference abs",
    "Geometry / Normal Reliability","Region / Geometry Boundary Weight","Material Source / Reflection Weight","Lighting Residual Weight","Dark / Saturation Signal Weight","Shadow Risk (heuristic)",
    "Final Relighting Confidence (heuristic)","Raw Log Ratio","Effective Log Ratio","Clamp Mask","Protection (white = keep source)",
    "Intrinsic Albedo (linear)","Intrinsic Original Diffuse Shading","Intrinsic Non-diffuse Residual (gray=0)","Intrinsic Uncertainty A/S/R","Intrinsic Recomposition Error x4","Intrinsic Validity","Intrinsic Diffuse Support","Protected Non-diffuse Fraction",
    "Supported Specular Candidate","Specular Behavior Confidence","Original-based Diffuse Anchor","Calculated Old GGX","Calculated New GGX","Bounded Specular Delta (gray=0)","Candidate Clipping Error x10","Specular Final Difference","Protected Residual (gray=0)",
    "Old Shadow Candidate","Observed Direct Visibility","Depth-shell Occlusion Support","Old Shadow Confidence","Unknown / Protected","Manual Shadow Confirmation","Manual Shadow Protection","Effective Shadow Support","Source / Shadow Support Overlay","Cast Old Shadow Map","Cast New Shadow Map","Cast Old Visibility","Cast New Visibility","Cast Observed Old Visibility","Effective Cast Confidence","Cast Change (gray=0)","Cast Difference from Previous Stage x4","Final with Paired Cast Shadows","Previous Stage / No Cast Change","Fog Ray Distance / max","Fog Effective Transmittance","Fog Confidence","Fog Airlight Contribution (linear)"};
inline constexpr int ImageDebugCount=78;
}
