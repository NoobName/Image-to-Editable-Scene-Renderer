#pragma once
#include "Scene/Scene.h"
#include "Renderer/RenderSettings.h"
#include "Scene/SceneEditing.h"
namespace isr {
enum class SelectionKind { Camera,Entity,Light,Environment };
struct SceneSelection { SelectionKind kind=SelectionKind::Entity;size_t index=0; };
void DrawSceneHierarchy(const Scene&,SceneSelection&);
void DrawEntityInspector(Scene&,size_t,SceneEditState&,RenderSettings&);
void DrawCameraInspector(Camera&);
void DrawLightInspector(Light&,RenderSettings&);
void DrawDisplaySettings(RenderSettings&);
bool FrameScene(Scene&,const SceneSelection* selection=nullptr);
}
