#pragma once
#include "App/InputState.h"
#include "Scene/Camera.h"
namespace isr {
class CameraController {
public:
    void Update(Camera&, const InputState&, float deltaSeconds);
    float Speed() const { return speed_; }
private:
    float speed_ = 3;
};
}
