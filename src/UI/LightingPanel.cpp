#include "UI/LightingPanel.h"
#include <imgui.h>
namespace isr {
namespace {
void Controls(LightingParameters& p){
    ImGui::DragFloat3("Travel direction",p.direction.data(),.01f,-1,1,"%.3f");
    ImGui::ColorEdit3("Direct color",p.directColor.data());ImGui::SliderFloat("Direct gain",&p.directIntensity,0,8);
    ImGui::ColorEdit3("Ambient color",p.ambientColor.data());ImGui::SliderFloat("Ambient gain",&p.ambientIntensity,0,8);
}
void Arrow(const LightingParameters& p){
    const auto start=ImGui::GetCursorScreenPos();ImGui::Dummy({90,50});const ImVec2 center{start.x+45,start.y+25};
    const ImVec2 end{center.x-p.direction[0]*35,center.y+p.direction[1]*22}; // Screen Y down; camera Y up.
    auto* draw=ImGui::GetWindowDrawList();draw->AddCircle(center,22,IM_COL32(120,120,120,255));
    draw->AddLine(center,end,IM_COL32(255,220,80,255),3);draw->AddCircleFilled(end,3,IM_COL32(255,220,80,255));
    ImGui::SameLine();ImGui::Text("TO-light XY\ntravel Z = %.3f",p.direction[2]);
}
}
bool DrawApplySourceButton(LightingSession& state){
    if(ImGui::Button("Apply source calibration"))return state.ApplySource();return false;
}
int DrawLightingPanel(RelightingSession& session,bool busy){
    ImGui::SeparatorText("Original lighting baseline");int action=0;
    const bool hasMaps=session.Source()&&session.Source()->analysisMaps;
    ImGui::BeginDisabled(busy||!hasMaps);if(ImGui::Button("Fit saved observations"))action=1;ImGui::EndDisabled();
    auto& state=session.lighting;
    if(!state.available){ImGui::TextWrapped("Unavailable: no lighting/lighting.json. Scene default lights are not estimates.");return action;}
    const auto& data=session.Source()->lighting->metadata;const auto& fit=data["fit"];
    if(!state.cacheValid)ImGui::TextWrapped("Historical fit metrics below: current calibration has not been evaluated.");
    ImGui::TextWrapped("%s | %s",fit["backend"].get<std::string>().c_str(),fit["status"].get<std::string>().c_str());
    ImGui::Text("Confidence %.3f (heuristic)",fit["confidence"].get<double>());
    ImGui::Text("Support %.1f%%",100*fit["validFraction"].get<double>());
    ImGui::Text("RMSE %.4f -> %.4f",fit["beforeRMSE"].get<double>(),fit["afterRMSE"].get<double>());
    ImGui::Text("Proxy median %.5f",fit["normalization"].get<double>());
    ImGui::TextWrapped("Relative scale; exposure=0, albedo gain=1. No lux/candela or target RGB output.");
    for(const auto& reason:fit["reasons"])ImGui::TextWrapped("%s",reason.get<std::string>().c_str());
    if(state.manualSource)ImGui::TextWrapped("Source: manual calibration (session only until exported).");
    else ImGui::TextWrapped("Source: %s",data["sourceLighting"]["provenance"].get<std::string>().c_str());
    Arrow(state.source);
    if(!state.cacheValid)ImGui::TextWrapped("Old Shading / Residual unavailable: source changed. Export calibration to rebuild evidence.");
    if(ImGui::CollapsingHeader("Source calibration")){
        ImGui::PushID("source");Controls(state.draft);
        DrawApplySourceButton(state);
        if(ImGui::Button("Discard source draft"))state.draft=state.source;
        ImGui::BeginDisabled(busy||!state.manualSource);if(ImGui::Button("Export calibrated source"))action=2;ImGui::EndDisabled();
        ImGui::TextWrapped("Apply invalidates old shading evidence and leaves target unchanged. Zero direction is rejected.");ImGui::PopID();
    }
    if(ImGui::CollapsingHeader("Target lighting (inspection only)")){
        ImGui::PushID("target");Controls(state.target);Arrow(state.target);
        if(ImGui::Button("Copy source to target"))state.target=state.source;
        ImGui::TextWrapped("Target edits do not change source, fit or 3D scene lights.");ImGui::PopID();
    }
    return action;
}
}
