#include "UI/LightingPanel.h"
#include <imgui.h>
namespace isr {
namespace {
void Controls(LightingParameters& p){
    ImGui::DragFloat3("Travel direction",p.direction.data(),.01f,-1,1,"%.3f");
    ImGui::ColorEdit3("Direct color",p.directColor.data());ImGui::SliderFloat("Direct gain",&p.directIntensity,0,8);
    ImGui::ColorEdit3("Ambient color",p.ambientColor.data());ImGui::SliderFloat("Environment fill",&p.ambientIntensity,0,8);
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
bool DrawResetTargetButton(LightingSession& state){
    if(!ImGui::Button("Reset target to source"))return false;state.target=state.source;state.targetGlobalGain=1;return true;
}
bool DrawCastShadowToggle(RelightingParameters& parameters){return ImGui::Checkbox("Paired cast shadows",&parameters.castShadows);}
bool DrawImageFogToggle(ImageFogParameters& parameters){return ImGui::Checkbox("Add image atmosphere",&parameters.enabled);}
int DrawLightingPanel(RelightingSession& session,bool busy){
    if(ImGui::CollapsingHeader("Additional image atmosphere",session.fog.enabled?ImGuiTreeNodeFlags_DefaultOpen:0)){
        auto& fog=session.fog;DrawImageFogToggle(fog);
        ImGui::SliderFloat("Additional density",&fog.density,0,10,"%.4f",ImGuiSliderFlags_AlwaysClamp);
        ImGui::ColorEdit3("Airlight (linear RGB)",fog.airlight.data(),ImGuiColorEditFlags_Float);
        ImGui::Checkbox("Allow relative / synthetic scale",&fog.allowRelativeScale);
        if(ImGui::Button("Reset additional atmosphere"))fog={};
        if(session.Source()&&session.Source()->analysisMaps){const auto& maps=session.Source()->analysisMaps->metadata;
            ImGui::TextWrapped("Geometry scale: %s. Density is inverse geometry units.",maps["camera"]["scaleType"].get<std::string>().c_str());}
        else ImGui::TextWrapped("Unavailable: no validated analysis geometry.");
        ImGui::TextWrapped("Source additional fog = 0 (fixed). Adds haze; does not remove baked fog. Known sky/invalid/unknown scale and protected regions are unchanged. Relative scale requires explicit enable. Native limit: 8M pixels. Applied before display Exposure, independent of lighting strength.");
    }
    ImGui::SeparatorText("SOURCE / TARGET lighting");int action=0;
    if(ImGui::CollapsingHeader("Offline analysis / rebuild")){
    const bool hasMaps=session.Source()&&session.Source()->analysisMaps;
    ImGui::BeginDisabled(busy||!hasMaps);if(ImGui::Button("Fit saved observations"))action=1;ImGui::EndDisabled();
    ImGui::BeginDisabled(busy||!hasMaps);if(ImGui::Button("Fit intrinsic-assisted diffuse"))action=4;ImGui::EndDisabled();
    ImGui::BeginDisabled(busy||!hasMaps);if(ImGui::Button("Estimate saved image intrinsics"))action=3;ImGui::EndDisabled();
    ImGui::BeginDisabled(busy||!hasMaps);if(ImGui::Button("Analyze old-shadow support"))action=5;ImGui::EndDisabled();
    }
    auto& state=session.lighting;
    if(!state.available){ImGui::TextWrapped("Unavailable: no lighting/lighting.json. Scene default lights are not estimates.");return action;}
    const auto& data=session.Source()->lighting->metadata;const auto& fit=data["fit"];
    if(!state.cacheValid)ImGui::TextWrapped("Historical fit metrics below: current calibration has not been evaluated.");
    ImGui::TextWrapped("%s | %s",fit["backend"].get<std::string>().c_str(),fit["status"].get<std::string>().c_str());
    ImGui::Text("Confidence %.3f (heuristic)",fit["confidence"].get<double>());
    ImGui::Text("Support %.1f%%",100*fit["validFraction"].get<double>());
    ImGui::Text("RMSE %.4f -> %.4f",fit["beforeRMSE"].get<double>(),fit["afterRMSE"].get<double>());
    ImGui::Text("Proxy median %.5f",fit["normalization"].get<double>());
    if(data.contains("assistance")&&ImGui::CollapsingHeader("Analysis provenance / errors")){const auto& a=data["assistance"];
        ImGui::TextWrapped("Intrinsic fit: %s",a["reason"].get<std::string>().c_str());
        ImGui::Text("Global scale %.5f / color gains 1",a["calibrationScale"].get<double>());
        ImGui::Text("Own-observation RMSE baseline %.4f / assisted %.4f",a["baselineFit"]["afterRMSE"].get<double>(),a["candidateFit"]["afterRMSE"].get<double>());
        ImGui::TextWrapped("Different observations: compare per-region common-gauge errors in lighting.json, not these two RMSE values alone.");
    }
    ImGui::TextWrapped("Relative scale; exposure=0, albedo gain=1. Not lux/candela.");
    if(ImGui::CollapsingHeader("Fit limitations"))for(const auto& reason:fit["reasons"])ImGui::TextWrapped("%s",reason.get<std::string>().c_str());
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
    if(ImGui::CollapsingHeader("Target lighting / relighting",ImGuiTreeNodeFlags_DefaultOpen)){
        ImGui::PushID("target");
        ImGui::SliderFloat("Global intensity",&state.targetGlobalGain,0,4,"%.2f",ImGuiSliderFlags_AlwaysClamp);
        if(ImGui::IsItemHovered())ImGui::SetTooltip("Multiplies both target direct and ambient base gains below. Source and display Exposure remain unchanged.");
        Controls(state.target);Arrow(state.target);
        if(ImGui::Button("Copy source to target")){state.target=state.source;state.targetGlobalGain=1;}
        DrawResetTargetButton(state);
        ImGui::SliderFloat("Relighting strength",&session.relighting.strength,0,1);
        int color=int(session.relighting.colorMode);const char* modes[]{"Luminance (preserve color)","Bounded colored illumination"};
        if(ImGui::Combo("Color response",&color,modes,2))session.relighting.colorMode=static_cast<RatioColorMode>(color);
        ImGui::Checkbox("Use stability controls",&session.relighting.stability);
        ImGui::Checkbox("Intrinsic diffuse protection",&session.relighting.intrinsicProtection);
        ImGui::Checkbox("Conservative specular handling",&session.relighting.specularEnabled);
        ImGui::SliderFloat("Specular edit strength",&session.relighting.specularStrength,0,1);
        ImGui::SliderFloat("Target roughness response",&session.relighting.specularRoughnessScale,.5f,2);
        DrawCastShadowToggle(session.relighting);
        ImGui::SliderFloat("Cast shadow strength",&session.relighting.shadowStrength,0,1);
        ImGui::SliderFloat("Shadow confidence scale",&session.relighting.shadowConfidence,0,1);
        ImGui::SliderInt("Image shadow PCF radius",&session.relighting.shadowPcfRadius,0,2);
        ImGui::SliderFloat("Image shadow depth bias",&session.relighting.shadowBias,0,.01f,"%.5f");
        ImGui::SliderFloat("Image shadow normal bias",&session.relighting.shadowNormalBias,0,.1f,"%.4f");
        ImGui::TextWrapped("Fixed observed source shell only. Old-shadow removal needs photo + geometry agreement; hidden/offscreen blockers, deep black and unknown regions remain unsupported. Turning off restores the previous composition.");
        if(ImGui::Button("Reset specular response")){session.relighting.specularStrength=.25f;session.relighting.specularRoughnessScale=1;}
        ImGui::TextWrapped("Directional dielectric highlights only. Metallic/glass/emission/unknown remain protected; disable to restore diffuse-only result. Source recalibration disables stale separation until refit.");
        if(ImGui::Button("Conservative preset"))session.relighting.Conservative();
        if(ImGui::Button("Reset response controls"))session.relighting={};
        if(ImGui::TreeNode("Protection range / material response")){
            ImGui::SliderFloat("Minimum ratio",&session.relighting.minRatio,.1f,1);
            ImGui::SliderFloat("Maximum ratio",&session.relighting.maxRatio,1,8);
            ImGui::SliderFloat("Chroma limit",&session.relighting.chromaLimit,1,2);
            ImGui::TextWrapped("Material response uses existing roughness scaling and bounded specular strength. Environment fill is ambient, not full reflections. Additional image atmosphere has separate controls.");ImGui::TreePop();}
        ImGui::Text("epsilon=%.3f ratio=[%.2f,%.2f]",session.relighting.epsilon,session.relighting.minRatio,session.relighting.maxRatio);
        ImGui::TextWrapped("Independent heuristic weights; validity is a hard gate. Imported PNG: white preserves original. Cast shadows require validated shadow evidence; unobserved shadows/reflections remain.");
        ImGui::TextWrapped("Target edits do not change source, fit or 3D scene lights.");ImGui::PopID();
    }
    return action;
}
}
