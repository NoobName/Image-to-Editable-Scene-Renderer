#include "UI/RecipePanel.h"
#include <imgui.h>
namespace isr {
void RecipePanel::Draw(bool available,bool reconstructionBusy){
    if(!actions_)return;auto& a=*actions_;
    try{if(auto path=dialog_.Poll();path&&!path->empty()){a.path=*path;a.request=pending_;}}
    catch(const std::exception& e){a.status=e.what();}
    ImGui::TextWrapped("Recipe v2 | immutable source + calibrated baseline + target + masks + additional image atmosphere (v1 readable)");
    ImGui::BeginDisabled(a.busy||dialog_.Busy()||reconstructionBusy);
    if(ImGui::Button("Open recipe...")){pending_=RecipeAction::Open;dialog_.Recipe(owner_,false);}
    ImGui::BeginDisabled(!available);
    if(ImGui::Button("Save new recipe...")){pending_=RecipeAction::SaveNew;dialog_.Recipe(owner_,true);}
    if(ImGui::Button("Replace recipe...")){pending_=RecipeAction::Replace;dialog_.Recipe(owner_,true);}
    ImGui::TextWrapped("Replace is explicit; Save new rejects an existing file. Native export: <=8192/edge, <=8 MiPixels, no implicit resize.");
    ImGui::InputText("Export directory",exportPath_.data(),exportPath_.size());
    if(ImGui::Button("Export native PNG + float DDS")&&exportPath_[0]){a.path=std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(exportPath_.data())));a.request=RecipeAction::Export;}
    ImGui::EndDisabled();ImGui::EndDisabled();
    if(a.busy&&ImGui::Button("Cancel recipe load"))a.cancel=true;
    ImGui::Separator();ImGui::TextWrapped("%s",a.status.c_str());
    ImGui::TextWrapped("PNG is display-referred sRGB. DDS is linear display-referred, not recovered HDR radiance. UI overlays and fit/pan/zoom are excluded.");
}
}
