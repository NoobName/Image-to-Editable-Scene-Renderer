#pragma once
#include <DirectXMath.h>
#include "Scene/Material.h"
namespace isr {
struct UvConstants { DirectX::XMFLOAT4 offsetScale,rotationSet; };
struct ObjectConstants {
    DirectX::XMFLOAT4X4 worldViewProjection,world,normalMatrix;
    DirectX::XMFLOAT4 baseColor,emissiveCutoff,factors,flags,extras;
    UvConstants uv[MaterialTextureCount];
};
static_assert(sizeof(ObjectConstants)==464);
inline constexpr unsigned MaxLights=8;
struct LightConstants { DirectX::XMFLOAT4 positionType,directionRange,colorIntensity; };
struct FrameConstants {
    DirectX::XMFLOAT4 cameraMode,ambientCount,nearFar;
    LightConstants lights[MaxLights];
    DirectX::XMFLOAT4X4 lightViewProjection;
    DirectX::XMFLOAT4 shadowParams,shadowInfo,environment;
};
static_assert(sizeof(FrameConstants)==544);
}
