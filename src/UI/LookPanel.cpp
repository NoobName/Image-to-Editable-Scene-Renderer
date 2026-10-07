#include "UI/ScenePanels.h"
#include "UI/ChineseText.h"
#include <imgui.h>
namespace isr {
namespace {
void Slider(LookParameters& look,std::string_view key){
    for(const auto& p:LookParameterSchema)if(p.key==key){
        float& value=look.*(p.member);const float before=value;
        const auto label=ChineseText(p.label)+"###"+std::string(p.label);
        ImGui::SliderFloat(label.c_str(),&value,p.minimum,p.maximum,"%.3f",ImGuiSliderFlags_AlwaysClamp);
        if(!std::isfinite(value))value=before;
        return;
    }
}
}
void DrawDisplaySettings(RenderSettings& settings){
    auto& look=settings.look;
    if(ImGui::Button("重置画面调整###Reset all look"))look=LookParameters{};
    ImGui::TextWrapped("效果仅用于最终画面；数值视图不经过画面调整。");
    if(ImGui::CollapsingHeader("色彩管理###Color Management",ImGuiTreeNodeFlags_DefaultOpen)){
        int tone=static_cast<int>(look.toneMapping);
        if(ImGui::Combo("色调映射###Tone Mapping",&tone,ToneMappingNames,3))look.toneMapping=static_cast<ToneMapping>(tone);
        Slider(look,"exposure");Slider(look,"gamma");
        ImGui::SetItemTooltip("艺术伽马：1 为中性值。始终只应用一次精确 sRGB 编码。");
        if(ImGui::Button("重置显示###Reset display")){look.exposure=0;look.gamma=1;look.toneMapping=ToneMapping::ACES;}
    }
    if(ImGui::CollapsingHeader("色彩调整###Color Grading",ImGuiTreeNodeFlags_DefaultOpen)){
        Slider(look,"temperature");ImGui::SetItemTooltip("-1 偏冷，+1 偏暖；这是艺术色彩平衡，不是开尔文色温。");
        Slider(look,"tint");ImGui::SetItemTooltip("-1 偏绿，+1 偏洋红。");
        Slider(look,"saturation");Slider(look,"contrast");
        ImGui::SetItemTooltip("在色调映射之前，以 18%% 线性灰为中心调整对比度。");
    }
    if(ImGui::CollapsingHeader("泛光###Bloom",ImGuiTreeNodeFlags_DefaultOpen)){
        ImGui::PushID("Bloom");ImGui::Checkbox("启用###Enabled",&look.bloomEnabled);ImGui::BeginDisabled(!look.bloomEnabled);
        Slider(look,"bloom-intensity");Slider(look,"bloom-threshold");
        ImGui::SetItemTooltip("阈值采用曝光前的场景线性 HDR 数值。");
        Slider(look,"bloom-knee");Slider(look,"bloom-radius");ImGui::EndDisabled();ImGui::PopID();
    }
    if(ImGui::CollapsingHeader("暗角###Vignette")){
        ImGui::PushID("Vignette");Slider(look,"vignette");Slider(look,"vignette-radius");Slider(look,"vignette-softness");ImGui::PopID();
    }
    if(ImGui::TreeNode("高级补光###Advanced fill")){ImGui::SliderFloat("环境补光###Ambient",&settings.ambient,0,0.5f,"%.3f");ImGui::TreePop();}
}
}
