#pragma once
#include "Scene/SceneEditing.h"
#include "Renderer/RenderSettings.h"
#include <string>
namespace isr {
// Deterministic runtime edits for GPU regression captures. Uses the same Scene data as the UI.
class EditorSmoke {
public:
    std::string mode,objectId;
    void Initialize(const Scene&);
    void Tick(unsigned frame,Scene&,RenderSettings&);
    std::optional<size_t> Selection()const{return root_;}
private:
    SceneEditState original_;
    std::optional<size_t> root_;
};
}
