#pragma once
#include "Renderer/EnvironmentManager.h"
#include "Renderer/RenderSettings.h"
#include <array>
namespace isr {
class EnvironmentPanel {
public:
    void Draw(RenderSettings&,const EnvironmentManager&);
private:
    std::array<char,2048> path_{};
};
}
