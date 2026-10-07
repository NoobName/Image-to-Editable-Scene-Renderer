#pragma once
#include "ScenePackage/RelightingRecipe.h"
#include "UI/RecipePanel.h"
#include "Renderer/Renderer.h"
#include "App/CameraController.h"
namespace isr {
class RecipeWorkflow {
public:
    RecipeActions actions;
    void Tick(Renderer&,Scene&,RenderSettings&,CameraController&,bool reconstructionBusy);
private:
    std::future<RelightingRecipe> loading_;
    std::optional<RelightingRecipe> pending_;
    uint64_t sceneRevision_{};
};
}
