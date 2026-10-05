#pragma once
#include "App/InputState.h"
#include "Scene/Camera.h"
#include "Scene/WorkMode.h"
namespace isr {
class CameraController {
public:
    void Update(Camera&, const InputState&, float deltaSeconds,WorkMode mode=WorkMode::Scene3D);
    float Speed() const { return speed_; }
private:
    float speed_ = 3;
};
}
