#pragma once
#include "UI/ScenePanels.h"
#include "Scene/SceneEditing.h"
#include <imgui.h>
namespace isr {
class ViewportTools {
public:
    bool Draw(Scene&,SceneSelection&,SceneEditState&,ImVec2 minimum,ImVec2 maximum,bool imageHovered);
    void Reset(){dragLight_.reset();}
private:
    std::optional<size_t> dragLight_;
};
}
