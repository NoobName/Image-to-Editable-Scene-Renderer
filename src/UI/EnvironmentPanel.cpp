#include "UI/EnvironmentPanel.h"
#include "Assets/AssetIO.h"
#include <imgui.h>
namespace isr {
void EnvironmentPanel::Draw(RenderSettings& settings,const EnvironmentManager& manager){
    if(ImGui::CollapsingHeader("环境###Environment",ImGuiTreeNodeFlags_DefaultOpen)){
        const auto label=PathUtf8(manager.Path().filename());
        if(ImGui::BeginCombo("HDR 环境图###HDRI",label.c_str())){
            for(const auto& path:manager.Available()){auto name=PathUtf8(path.filename());ImGui::PushID(PathUtf8(path).c_str());
                if(ImGui::Selectable(name.c_str(),path==manager.Path()))settings.environmentPath=path;ImGui::PopID();}
            ImGui::EndCombo();
        }
        ImGui::Checkbox("环境图照明###IBL lighting",&settings.ibl);ImGui::SameLine();ImGui::Checkbox("天空盒###Skybox",&settings.skybox);
        ImGui::SliderFloat("环境强度###Env intensity",&settings.environmentIntensity,0,5,"%.2f");
        float degrees=settings.environmentRotation*180/DirectX::XM_PI;
        if(ImGui::SliderFloat("环境旋转###Env rotation",&degrees,-180,180,"%.1f 度"))settings.environmentRotation=degrees*DirectX::XM_PI/180;
        ImGui::InputTextWithHint("##hdrpath",".hdr 文件路径（UTF-8）",path_.data(),path_.size());
        if(ImGui::Button("加载 HDR###Load HDR")&&path_[0])settings.environmentPath=std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(path_.data())));
        if(!manager.Error().empty())ImGui::TextWrapped("加载失败：%s",manager.Error().c_str());
        ImGui::TextDisabled("HDR 环境光照与反射");
    }
    ImGui::Separator();
}
}
