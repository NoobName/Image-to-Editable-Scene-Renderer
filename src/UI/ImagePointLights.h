#pragma once
#include "ScenePackage/RelightingSession.h"
#include "UI/PointLightGizmo.h"
namespace isr {
bool CanEditImagePoints(const RelightingSession&);
PointCamera ImagePointCamera(const SourceObservation&);
std::optional<ImagePointLight> ImagePointAt(const RelightingSession&,float u,float v);
void DrawImagePointPanel(RelightingSession&);
bool DrawImagePointAddButton(RelightingSession&);
bool DrawImagePointHandles(RelightingSession&,const PointCanvas&,ImVec2 clipMin,ImVec2 clipMax,bool hovered);
void DrawImagePointOverlays(const RelightingSession&,const PointCanvas&,bool hovered);
}
