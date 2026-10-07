#include "UI/ChineseText.h"
#include "UI/ScenePanels.h"
#include <imgui.h>
#include <functional>
namespace isr {
void DrawSceneHierarchy(const Scene& scene,SceneSelection& selection){
    ImGui::TextDisabled("图像场景 / 视觉开发");ImGui::Separator();
    if(ImGui::Selectable("相机###Camera",selection.kind==SelectionKind::Camera))selection={SelectionKind::Camera,0};
    if(ImGui::TreeNodeEx("物体###Objects",ImGuiTreeNodeFlags_DefaultOpen)){
        std::vector<std::vector<size_t>> children(scene.entities.size()+1);
        for(size_t i=0;i<scene.entities.size();++i)children[scene.entities[i].parent.value_or(scene.entities.size())].push_back(i);
        std::function<void(size_t)> draw=[&](size_t index){
            const auto& entity=scene.entities[index];ImGui::PushID(static_cast<int>(index));
            auto flags=ImGuiTreeNodeFlags_OpenOnArrow|ImGuiTreeNodeFlags_SpanAvailWidth;
            if(children[index].empty())flags|=ImGuiTreeNodeFlags_Leaf;
            if(selection.kind==SelectionKind::Entity&&selection.index==index)flags|=ImGuiTreeNodeFlags_Selected;
            const bool open=ImGui::TreeNodeEx("##entity",flags,"%s",entity.name.empty()?"未命名节点":ChineseText(entity.name).c_str());
            if(ImGui::IsItemClicked()&&!ImGui::IsItemToggledOpen())selection={SelectionKind::Entity,index};
            if(open){for(auto child:children[index])draw(child);ImGui::TreePop();}ImGui::PopID();
        };
        for(auto root:children[scene.entities.size()])draw(root);ImGui::TreePop();
    }
    if(ImGui::TreeNodeEx("光源###Lights",ImGuiTreeNodeFlags_DefaultOpen)){
        for(size_t i=0;i<scene.lights.size();++i){ImGui::PushID(static_cast<int>(i));
            const auto label=std::string(scene.lights[i].type==LightType::Point?"点光源 ":"方向光 ")+std::to_string(i);
            if(ImGui::Selectable(label.c_str(),selection.kind==SelectionKind::Light&&selection.index==i))selection={SelectionKind::Light,i};
            ImGui::PopID();}ImGui::TreePop();
    }
    if(ImGui::Selectable("环境###Environment",selection.kind==SelectionKind::Environment))selection={SelectionKind::Environment,0};
    ImGui::Spacing();ImGui::Separator();
    ImGui::TextDisabled("%zu 个网格",scene.meshes.size());ImGui::TextDisabled("%zu 个材质",scene.materials.size());
    ImGui::TextDisabled("%zu 张纹理",scene.textures.size());
    ImGui::Spacing();ImGui::TextWrapped("选择节点以编辑其局部变换和材质。");
}
}
