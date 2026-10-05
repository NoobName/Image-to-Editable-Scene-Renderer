#include "UI/EnvironmentPanel.h"
#include "Assets/AssetIO.h"
#include <imgui.h>
namespace isr {
void EnvironmentPanel::Draw(RenderSettings& settings,const EnvironmentManager& manager){
    if(ImGui::CollapsingHeader("Environment",ImGuiTreeNodeFlags_DefaultOpen)){
        const auto label=PathUtf8(manager.Path().filename());
        if(ImGui::BeginCombo("HDRI",label.c_str())){
            for(const auto& path:manager.Available()){auto name=PathUtf8(path.filename());ImGui::PushID(PathUtf8(path).c_str());
                if(ImGui::Selectable(name.c_str(),path==manager.Path()))settings.environmentPath=path;ImGui::PopID();}
            ImGui::EndCombo();
        }
        ImGui::Checkbox("IBL lighting",&settings.ibl);ImGui::SameLine();ImGui::Checkbox("Skybox",&settings.skybox);
        ImGui::SliderFloat("Env intensity",&settings.environmentIntensity,0,5,"%.2f");
        float degrees=settings.environmentRotation*180/DirectX::XM_PI;
        if(ImGui::SliderFloat("Env rotation",&degrees,-180,180,"%.1f deg"))settings.environmentRotation=degrees*DirectX::XM_PI/180;
        ImGui::InputTextWithHint("##hdrpath","Path to .hdr (UTF-8)",path_.data(),path_.size());
        if(ImGui::Button("Load HDR")&&path_[0])settings.environmentPath=std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(path_.data())));
        if(!manager.Error().empty())ImGui::TextWrapped("Load failed: %s",manager.Error().c_str());
        ImGui::TextDisabled("HDR lighting and reflections");
    }
    ImGui::Separator();
}
}
