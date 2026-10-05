#include "UI/ScenePanels.h"
#include <imgui.h>
namespace isr {
namespace {
void Slider(LookParameters& look,std::string_view key){
    for(const auto& p:LookParameterSchema)if(p.key==key){
        float& value=look.*(p.member);const float before=value;
        ImGui::SliderFloat(p.label.data(),&value,p.minimum,p.maximum,"%.3f",ImGuiSliderFlags_AlwaysClamp);
        if(!std::isfinite(value))value=before;
        return;
    }
}
}
void DrawDisplaySettings(RenderSettings& settings){
    auto& look=settings.look;
    if(ImGui::Button("Reset all look"))look=LookParameters{};
    ImGui::TextWrapped("Effects apply to Final. Data views bypass look adjustments.");
    if(ImGui::CollapsingHeader("Color Management",ImGuiTreeNodeFlags_DefaultOpen)){
        int tone=static_cast<int>(look.toneMapping);
        if(ImGui::Combo("Tone Mapping",&tone,ToneMappingNames,3))look.toneMapping=static_cast<ToneMapping>(tone);
        Slider(look,"exposure");Slider(look,"gamma");
        ImGui::SetItemTooltip("Creative gamma: 1 is neutral. Exact sRGB encoding is always applied once.");
        if(ImGui::Button("Reset display")){look.exposure=0;look.gamma=1;look.toneMapping=ToneMapping::ACES;}
    }
    if(ImGui::CollapsingHeader("Color Grading",ImGuiTreeNodeFlags_DefaultOpen)){
        Slider(look,"temperature");ImGui::SetItemTooltip("-1 cooler / +1 warmer. Artistic balance, not Kelvin.");
        Slider(look,"tint");ImGui::SetItemTooltip("-1 greener / +1 more magenta.");
        Slider(look,"saturation");Slider(look,"contrast");
        ImGui::SetItemTooltip("Contrast around 18%% linear gray, before tone mapping.");
    }
    if(ImGui::CollapsingHeader("Bloom",ImGuiTreeNodeFlags_DefaultOpen)){
        ImGui::PushID("Bloom");ImGui::Checkbox("Enabled",&look.bloomEnabled);ImGui::BeginDisabled(!look.bloomEnabled);
        Slider(look,"bloom-intensity");Slider(look,"bloom-threshold");
        ImGui::SetItemTooltip("Threshold in scene-linear HDR, before exposure.");
        Slider(look,"bloom-knee");Slider(look,"bloom-radius");ImGui::EndDisabled();ImGui::PopID();
    }
    if(ImGui::CollapsingHeader("Vignette")){
        ImGui::PushID("Vignette");Slider(look,"vignette");Slider(look,"vignette-radius");Slider(look,"vignette-softness");ImGui::PopID();
    }
    if(ImGui::TreeNode("Advanced fill")){ImGui::SliderFloat("Ambient",&settings.ambient,0,0.5f,"%.3f");ImGui::TreePop();}
}
}
