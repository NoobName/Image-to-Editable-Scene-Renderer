#include "UI/ReferencePanel.h"
#include "Assets/AssetIO.h"
#include <imgui.h>
namespace isr {
bool DrawApplyReferenceButton(RelightingSession& session){return ImGui::Button("Apply target proposal")&&ApplyReferenceProposal(session);}
bool DrawResetReferenceButton(RelightingSession& session){return ImGui::Button("Restore target before reference")&&ResetReferenceTarget(session);}
bool DrawOptimizeTargetButton(const RelightingSession& session,ReferenceActions& actions){
    if(!ImGui::Button("Optimize current target")||!session.reference.analysis)return false;
    actions.input=session.reference.analysis->root;actions.request=ReferenceAction::Optimize;return true;
}
void ReferencePanel::Draw(RelightingSession& session,bool otherBusy){
    if(!actions_)return;auto& a=*actions_;
    try{if(auto selected=picker_.Poll();selected&&!selected->empty()){
        const auto text=PathUtf8(*selected);strncpy_s(path_.data(),path_.size(),text.c_str(),_TRUNCATE);
        if(loadProposal_){a.input=*selected;a.request=ReferenceAction::Load;}
    }}catch(const std::exception& e){a.status=e.what();}
    ImGui::TextWrapped("Reference lighting | camera-relative proposal, no RGB content transfer");
    ImGui::BeginDisabled(a.busy||picker_.Busy()||otherBusy||!session.CanDisplayImage());
    ImGui::InputText("Image/package path",path_.data(),path_.size());
    if(ImGui::Button("Browse reference...")){loadProposal_=false;picker_.Reference(owner_);}
    ImGui::SameLine();if(ImGui::Button("Open proposal...")){loadProposal_=true;picker_.Recipe(owner_,false);}
    int relation=a.relation=="same-scene"?0:1;const char* relations[]{"Same scene (declared)","Different content / style"};
    if(ImGui::Combo("Relation",&relation,relations,2))a.relation=relation==0?"same-scene":"different-content";
    ImGui::Checkbox("Use configured cached backends",&a.useConfiguredBackends);
    ImGui::Checkbox("Allow labelled fallback",&a.allowFallback);
    ImGui::TextWrapped("Saved packages reuse observations. Raw images use File reconstruction settings, offline only. Fallback cannot claim a recovered direction.");
    if(ImGui::Button("Analyze reference")&&path_[0]){a.input=std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(path_.data())));a.request=ReferenceAction::Analyze;}
    ImGui::EndDisabled();if(a.busy&&ImGui::Button("Cancel reference analysis"))a.cancel=true;
    ImGui::TextWrapped("%s",a.status.c_str());if(!a.log.empty()&&ImGui::Button("Copy reference log path"))ImGui::SetClipboardText(PathUtf8(a.log).c_str());
    if(!session.reference.analysis)return;
    const auto& data=*session.reference.analysis;const auto& j=data.metadata;const auto& p=j.at("proposal");
    ImGui::SeparatorText("INSPECT BEFORE APPLY");ImGui::Checkbox("Show reference residual",&session.reference.showResidual);
    ImGui::Text("Confidence %.3f | %s",data.confidence,data.canApply?"proposal available":"unavailable");
    ImGui::TextWrapped("%s",p.at("relation").get_ref<const std::string&>().c_str());
    ImGui::TextWrapped("Reference camera -> source camera: XYZ aligned by convention. Shared world sun position is unknown.");
    const auto& current=session.lighting.EffectiveTarget();const auto& target=data.target;
    ImGui::Text("Travel: %.3f %.3f %.3f",target.direction[0],target.direction[1],target.direction[2]);
    ImGui::Text("Direct %.3f -> %.3f",current.directIntensity,target.directIntensity);ImGui::Text("Ambient %.3f -> %.3f",current.ambientIntensity,target.ambientIntensity);
    ImGui::Text("Fit RMSE %.4f -> %.4f",j.at("fit").at("beforeRMSE").get<double>(),j.at("fit").at("afterRMSE").get<double>());
    ImGui::TextWrapped("Apply changes New Shading, paired specular/shadow and ratio. Source, Exposure and Look remain fixed.");
    auto* draw=ImGui::GetWindowDrawList();const auto cursor=ImGui::GetCursorScreenPos();const ImVec2 c{cursor.x+50,cursor.y+35};draw->AddCircle(c,29,IM_COL32(160,160,160,255));
    draw->AddLine(c,{c.x-current.direction[0]*26,c.y+current.direction[1]*26},IM_COL32(50,190,255,255),2);
    draw->AddLine(c,{c.x-target.direction[0]*26,c.y+target.direction[1]*26},data.canApply?IM_COL32(255,210,60,255):IM_COL32(110,110,110,255),2);ImGui::Dummy({100,70});
    ImGui::TextWrapped("XY arrows point TO light; travel Z above. Blue=current target, yellow=proposal.");
    if(data.confidence<.5f)ImGui::TextWrapped("Low confidence: inspect residual and reasons; correct direction manually after applying if supported.");
    for(const auto& reason:p.at("reasons"))ImGui::TextWrapped("%s",reason.get_ref<const std::string&>().c_str());
    ImGui::TextWrapped("Unavailable: absolute radiance, shared world direction, HDR environment, relative exposure.");
    ImGui::SeparatorText("BOUNDED TARGET OPTIMIZATION");
    ImGui::Checkbox("Registered pixel alignment confirmed",&a.registered);
    ImGui::SliderInt("Iteration limit",&a.iterations,1,200);
    ImGui::TextWrapped("Fixed source, normal, albedo and Exposure. Same-scene requires registered pixels; different content uses illumination statistics only. Changes while running invalidate the result.");
    ImGui::BeginDisabled(a.busy||otherBusy||!data.canApply||!data.optimization.is_null());
    DrawOptimizeTargetButton(session,a);
    ImGui::EndDisabled();
    if(!data.optimization.is_null()){
        const auto& o=data.optimization;ImGui::TextWrapped("%s: %s",o.at("status").get_ref<const std::string&>().c_str(),o.at("reason").get_ref<const std::string&>().c_str());
        ImGui::Text("Loss %.6g -> %.6g",o.at("initialLoss").get<double>(),o.at("bestLoss").get<double>());
        std::vector<float> loss;for(const auto& h:o.at("history"))loss.push_back(h.at("bestLoss").get<float>());
        if(!loss.empty())ImGui::PlotLines("Fixed loss",loss.data(),int(loss.size()),0,nullptr,0,FLT_MAX,{0,90});
        ImGui::Text("DX12 verified: %s | error %.3g",session.reference.gpuVerified?"yes":"no",session.reference.gpuError);
        ImGui::TextWrapped("CPU evaluator covers diffuse New Shading. Inspect debug/optimization.png for trajectory/mask/residual; full specular/shadow/ratio output is checked through native export.");
    }
    ImGui::BeginDisabled(a.busy||otherBusy||!data.canApply);
    try{DrawApplyReferenceButton(session);if(session.reference.applied)DrawResetReferenceButton(session);}
    catch(const std::exception& e){a.status=e.what();}ImGui::EndDisabled();
}
}
