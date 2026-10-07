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
inline constexpr const char* ImageDebugNames[]={"原图","原图 / 像素网格","深度（相机 Z）","几何法线（相机左手系）","几何法线（世界左手系）",
    "位置（相机左手系）","有效性","区域标识","估计反照率（线性）","粗糙度","金属度","几何置信度","材质置信度","区域置信度","材质切线法线",
    "估计原始明暗（代理值 /4）","拟合旧明暗 /4","光照亮度残差 [0,1]","光照拟合蒙版",
    "计算旧明暗 /4","计算新明暗 /4","明暗绝对差 /4","法线与朝向光源点积（负值为红色）","明暗有效性",
    "重光照比率（1 为灰色）","原图重光照结果","重光照绝对差",
    "几何 / 法线可靠性","区域 / 几何边界权重","材质来源 / 反射权重","光照残差权重","暗部 / 饱和信号权重","阴影风险（启发式）",
    "最终重光照置信度（启发式）","原始对数比率","生效对数比率","限幅蒙版","保护（白色保留原图）",
    "本征反照率（线性）","本征原始漫反射明暗","本征非漫反射残差（灰色为 0）","本征不确定性：反照率 / 明暗 / 残差","本征重合成误差 ×4","本征有效性","本征漫反射支持度","受保护非漫反射比例",
    "受支持的高光候选","高光行为置信度","基于原图的漫反射基准","计算旧 GGX 高光","计算新 GGX 高光","有界高光变化（灰色为 0）","候选限幅误差 ×10","高光最终差异","受保护残差（灰色为 0）",
    "旧阴影候选","观测直射光可见性","深度几何遮挡支持度","旧阴影置信度","未知 / 受保护","手工确认阴影","手工保护阴影","生效阴影支持度","原图 / 阴影支持度叠加","旧投射阴影贴图","新投射阴影贴图","旧投射可见性","新投射可见性","观测旧投射可见性","生效投射阴影置信度","投射阴影变化（灰色为 0）","相较前阶段的投射阴影差异 ×4","成对投射阴影最终结果","前阶段 / 不改变投射阴影","雾：视线距离 / 最大值","雾：生效透射率","雾：置信度","雾：空气光贡献（线性）"};
inline constexpr int ImageDebugCount=78;
}
