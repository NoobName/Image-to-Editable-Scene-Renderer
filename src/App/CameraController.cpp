#include "App/CameraController.h"
#include <windows.h>
#include <algorithm>
#include <cmath>
namespace isr {
void CameraController::Update(Camera& camera, const InputState& input, float dt) {
    if (!input.active) return;
    if (input.keys['R']) { camera = Camera{}; return; }
    // Window only accumulates motion while RMB is held. Consume that motion
    // even when button-up arrived in the same pump as the final mouse move.
    if (input.mouseX!=0 || input.mouseY!=0) {
        if (input.keys[VK_MENU]) camera.Orbit(camera.OrbitTarget(),input.mouseX*0.004f,-input.mouseY*0.004f);
        else camera.Rotate(input.mouseX*0.004f,-input.mouseY*0.004f);
    }
    if (input.wheel) {
        if (input.rightMouse) speed_ = std::clamp(speed_*std::pow(1.2f,input.wheel),0.1f,100.0f);
        else camera.Dolly(input.wheel*0.4f*speed_);
    }
    float x = float(input.keys['D'])-float(input.keys['A']);
    float y = float(input.keys['E'])-float(input.keys['Q']);
    float z = float(input.keys['W'])-float(input.keys['S']);
    const float length = std::sqrt(x*x+y*y+z*z);
    if (length > 0) {
        const float distance = speed_*std::clamp(dt,0.0f,0.1f)*(input.keys[VK_SHIFT] ? 3.0f : 1.0f)/length;
        camera.Move(x*distance,y*distance,z*distance);
    }
}
}
