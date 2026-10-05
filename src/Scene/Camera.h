#pragma once
#include <DirectXMath.h>
namespace isr {
class Camera {
public:
    Camera();
    void SetPerspective(float verticalFovRadians, float aspect, float nearPlane, float farPlane);
    void SetAspect(float aspect) { SetPerspective(fov_, aspect, near_, far_); }
    void Move(float right, float up, float forward);
    void Rotate(float yawDelta, float pitchDelta);
    void Orbit(DirectX::XMFLOAT3 target, float yawDelta, float pitchDelta);
    void Dolly(float distance) { Move(0,0,distance); }
    void LookAt(DirectX::XMFLOAT3 position, DirectX::XMFLOAT3 target);
    void SetPose(DirectX::XMFLOAT3 position,float yaw,float pitch);
    DirectX::XMFLOAT2 Angles() const { return {yaw_,pitch_}; }
    DirectX::XMFLOAT3 OrbitTarget() const { return orbitTarget_; }
    float Aspect() const { return aspect_; }
    DirectX::XMMATRIX View() const;
    DirectX::XMMATRIX Projection() const;
    DirectX::XMFLOAT3 Position() const { return position_; }
    float Fov() const { return fov_; }
    float NearPlane() const { return near_; }
    float FarPlane() const { return far_; }
private:
    DirectX::XMVECTOR Forward() const;
    DirectX::XMFLOAT3 position_{0,2,-7};
    DirectX::XMFLOAT3 orbitTarget_{0,0.7f,0};
    float yaw_{}, pitch_{}, fov_ = DirectX::XM_PIDIV4, aspect_ = 16.0f / 9.0f, near_ = 0.1f, far_ = 200;
};
}
