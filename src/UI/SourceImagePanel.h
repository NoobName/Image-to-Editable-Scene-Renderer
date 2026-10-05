#pragma once
#include "ScenePackage/RelightingSession.h"
#include <imgui.h>
namespace isr {
bool DrawWorkModeControls(RelightingSession&);
void DrawSourceInformation(const RelightingSession&,uint32_t viewportWidth,uint32_t viewportHeight);
void DrawSourceCoordinates(const RelightingSession&,ImVec2 viewportOrigin,uint32_t viewportWidth,uint32_t viewportHeight,bool hovered);
}
