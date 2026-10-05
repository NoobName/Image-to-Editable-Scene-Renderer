#pragma once
#include <array>
namespace isr {
struct InputState {
    std::array<bool,256> keys{};
    bool rightMouse{}, active = true;
    float mouseX{}, mouseY{}, wheel{};
    void ClearDeltas() { mouseX = mouseY = wheel = 0; }
};
}
