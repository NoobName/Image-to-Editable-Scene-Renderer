#pragma once
#include "UI/ScenePanels.h"
#include "ScenePackage/ImagePointLight.h"
#include <imgui.h>
namespace isr {
void ScenePointAddButton(Scene&,PointLightInteraction&);
bool DrawScenePointLights(Scene&,SceneSelection&,SceneEditState&,PointLightInteraction&,ImVec2,ImVec2,bool);
void ScenePointInspector(Scene&,SceneSelection&,PointLightInteraction&);
}
