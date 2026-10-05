#pragma once
#include <DirectXMath.h>
namespace isr {
enum class LightType { Directional, Point };
struct Light {
    LightType type = LightType::Directional;
    DirectX::XMFLOAT3 direction{0.4f,-0.8f,0.5f}; // Direction in which light travels.
    DirectX::XMFLOAT3 position{0,3,0};
    DirectX::XMFLOAT3 color{1,1,1};
    float intensity = 3.0f;
    float range = 10;
};
}
