#pragma once
#include "ScenePackage/RelightingSession.h"
#include <imgui.h>
namespace isr {
ImageRect DisplayImageRect(uint32_t sw,uint32_t sh,float width,float height,const ImageDisplayState&);
void DrawImageToolbar(RelightingSession&);
void DrawOriginalReference(const RelightingSession&,ImTextureID,ImTextureID reference=0,ImTextureID residual=0);
void DrawImageCanvas(RelightingSession&,ImTextureID original,ImTextureID rendered,ImVec2 size);
void DrawImageProtection(RelightingSession&);
// Shared with event tests; all interactions go through ImGui's actual active/hovered state.
bool DrawImageSun(RelightingSession&,ImVec2 origin);
bool DrawApplyRegionProtection(RelightingSession&);
}
