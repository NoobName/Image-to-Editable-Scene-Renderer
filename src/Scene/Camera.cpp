#include "Scene/Camera.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace isr {
using namespace DirectX;
Camera::Camera() { LookAt({5,3.5f,-7},{0,0.7f,0}); }
void Camera::SetPerspective(float fov, float aspect, float nearPlane, float farPlane) {
    if (!(fov > 0 && fov < XM_PI && aspect > 0 && nearPlane > 0 && farPlane > nearPlane) ||
        !std::isfinite(fov + aspect + nearPlane + farPlane)) throw std::invalid_argument("Invalid perspective parameters");
    fov_ = fov; aspect_ = aspect; near_ = nearPlane; far_ = farPlane;
}
XMVECTOR Camera::Forward() const { return XMVectorSet(std::sin(yaw_)*std::cos(pitch_),std::sin(pitch_),std::cos(yaw_)*std::cos(pitch_),0); }
void Camera::Move(float right, float up, float forward) {
    const auto r = XMVectorSet(std::cos(yaw_),0,-std::sin(yaw_),0);
    XMStoreFloat3(&position_, XMLoadFloat3(&position_) + right*r + up*XMVectorSet(0,1,0,0) + forward*Forward());
}
void Camera::Rotate(float yawDelta, float pitchDelta) {
    yaw_ = std::remainder(yaw_ + yawDelta, XM_2PI);
    pitch_ = std::clamp(pitch_ + pitchDelta, -XM_PIDIV2 + 0.01f, XM_PIDIV2 - 0.01f);
}
void Camera::LookAt(XMFLOAT3 position, XMFLOAT3 target) {
    const auto delta = XMLoadFloat3(&target) - XMLoadFloat3(&position);
    if (XMVectorGetX(XMVector3LengthSq(delta)) < 1e-8f) throw std::invalid_argument("Camera position equals target");
    XMFLOAT3 direction; XMStoreFloat3(&direction, XMVector3Normalize(delta));
    orbitTarget_=target;position_ = position; yaw_ = std::atan2(direction.x, direction.z); pitch_ = 0;
    Rotate(0,std::asin(std::clamp(direction.y,-1.0f,1.0f)));
}
void Camera::SetPose(XMFLOAT3 position,float yaw,float pitch){
    if(!std::isfinite(position.x+position.y+position.z+yaw+pitch))throw std::invalid_argument("Nonfinite camera pose");
    position_=position;yaw_=0;pitch_=0;Rotate(yaw,pitch);
}
void Camera::Orbit(XMFLOAT3 target, float yawDelta, float pitchDelta) {
    const float distance = XMVectorGetX(XMVector3Length(XMLoadFloat3(&position_) - XMLoadFloat3(&target)));
    if (distance < 0.001f) return;
    LookAt(position_, target); Rotate(yawDelta,pitchDelta);
    XMStoreFloat3(&position_, XMLoadFloat3(&target) - distance*Forward());
}
XMMATRIX Camera::View() const { return XMMatrixLookToLH(XMLoadFloat3(&position_), Forward(), XMVectorSet(0,1,0,0)); }
XMMATRIX Camera::Projection() const { return XMMatrixPerspectiveFovLH(fov_,aspect_,near_,far_); }
}
