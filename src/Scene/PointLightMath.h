#pragma once
#include "Scene/Camera.h"
#include <optional>
#include <cmath>
namespace isr {
// Normalized image-edge coordinates: u right, v down; LH camera Y up, Z forward.
// Moving a handle intersects its cursor ray with a fixed camera-Z plane.
struct PointCamera {
    DirectX::XMFLOAT4X4 view{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    float fx=1,fy=1,cx=.5f,cy=.5f;
};
inline PointCamera PointCameraFor(const Camera& camera){
    PointCamera result;DirectX::XMStoreFloat4x4(&result.view,camera.View());
    DirectX::XMFLOAT4X4 p;DirectX::XMStoreFloat4x4(&p,camera.Projection());result.fx=p._11*.5f;result.fy=p._22*.5f;return result;
}
inline DirectX::XMFLOAT3 InPointCamera(const PointCamera& camera,DirectX::XMFLOAT3 p){
    DirectX::XMFLOAT3 result;DirectX::XMStoreFloat3(&result,DirectX::XMVector3TransformCoord(DirectX::XMLoadFloat3(&p),DirectX::XMLoadFloat4x4(&camera.view)));return result;
}
inline std::optional<DirectX::XMFLOAT2> ProjectPoint(const PointCamera& camera,DirectX::XMFLOAT3 p){
    const auto c=InPointCamera(camera,p);if(!std::isfinite(c.x+c.y+c.z)||c.z<.001f)return {};
    return DirectX::XMFLOAT2{camera.cx+camera.fx*c.x/c.z,camera.cy-camera.fy*c.y/c.z};
}
inline DirectX::XMFLOAT3 UnprojectPoint(const PointCamera& camera,float u,float v,float depth){
    DirectX::XMFLOAT3 result;const auto p=DirectX::XMVectorSet((u-camera.cx)*depth/camera.fx,(camera.cy-v)*depth/camera.fy,depth,1);
    DirectX::XMStoreFloat3(&result,DirectX::XMVector3TransformCoord(p,DirectX::XMMatrixInverse(nullptr,DirectX::XMLoadFloat4x4(&camera.view))));return result;
}
}
