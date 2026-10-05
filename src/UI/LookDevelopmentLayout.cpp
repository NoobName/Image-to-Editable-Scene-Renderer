#include "UI/LookDevelopmentUI.h"
#include <imgui.h>
#include <algorithm>
#include <cmath>
namespace isr {
namespace {
constexpr auto PanelFlags=ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoCollapse|
    ImGuiWindowFlags_NoSavedSettings;
void Place(float x,float y,float w,float h){ImGui::SetNextWindowPos({x,y});ImGui::SetNextWindowSize({w,h});}
}
void LookDevelopmentUI::DrawPanels(Scene& scene,RenderSettings& settings,EnvironmentManager& environment){
    const auto& io=ImGui::GetIO();const float w=io.DisplaySize.x,h=io.DisplaySize.y;
    const float left=std::min(220.0f,w*0.20f),right=std::min(355.0f,w*0.34f),middle=std::max(1.0f,w-left-right);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,0);ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,1);
    const auto previousSelection=selection_;
    Place(0,0,left,h);ImGui::Begin("Scene",nullptr,PanelFlags);DrawSceneHierarchy(scene,selection_);ImGui::End();
    Place(left,0,middle,h);ImGui::Begin("Viewport",nullptr,PanelFlags|ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
    int mode=static_cast<int>(settings.mode);ImGui::SetNextItemWidth(std::min(180.0f,middle*0.34f));
    if(ImGui::Combo("##renderMode",&mode,RenderModeNames,RenderModeCount))settings.mode=static_cast<RenderMode>(mode);
    if(ImGui::IsItemHovered())ImGui::SetTooltip("Original: source photo (magenta if unavailable). Estimated: raw maps without material factors.\nEstimated Normal: tangent map; check Inspector for fallback provenance.");
    ImGui::SameLine();const bool frameAll=ImGui::Button("Frame all");
    ImGui::SameLine();const bool frameSelected=ImGui::Button("Frame selected");
    const auto available=ImGui::GetContentRegionAvail();
    viewportWidth_=static_cast<uint32_t>(std::max(1.0f,std::floor(available.x)));
    viewportHeight_=static_cast<uint32_t>(std::max(1.0f,std::floor(available.y-ImGui::GetTextLineHeightWithSpacing()*2)));
    scene.camera.SetAspect(float(viewportWidth_)/float(viewportHeight_));
    if(frameAll)FrameScene(scene);else if(frameSelected)FrameScene(scene,&selection_);
    ImGui::Image(static_cast<ImTextureID>(heap_.Gpu(ViewportSlot).ptr),{float(viewportWidth_),float(viewportHeight_)});
    const auto min=ImGui::GetItemRectMin(),max=ImGui::GetItemRectMax();
    const bool hovered=ImGui::IsItemHovered();
    ImGui::TextDisabled("RMB look | WASDQE move | Wheel dolly | Alt+RMB orbit");
    ImGui::TextDisabled("%u x %u | %.1f FPS",viewportWidth_,viewportHeight_,io.Framerate);ImGui::End();
    Place(left+middle,0,right,h);ImGui::Begin("Inspector",nullptr,PanelFlags);
    ImGui::PushItemWidth(std::max(80.0f,right*0.52f));
    const bool selectionChanged=selection_.kind!=previousSelection.kind||selection_.index!=previousSelection.index;
    if(ImGui::BeginTabBar("InspectorTabs")){
    if(ImGui::BeginTabItem("Object",nullptr,selectionChanged?ImGuiTabItemFlags_SetSelected:0)){
    switch(selection_.kind){
    case SelectionKind::Entity:DrawEntityInspector(scene,selection_.index);break;
    case SelectionKind::Camera:DrawCameraInspector(scene.camera);break;
    case SelectionKind::Light:if(selection_.index<scene.lights.size())DrawLightInspector(scene.lights[selection_.index],settings);break;
    case SelectionKind::Environment:environmentPanel_.Draw(settings,environment);break;
    }
    ImGui::EndTabItem();}
    if(ImGui::BeginTabItem("Look")){DrawDisplaySettings(settings);ImGui::EndTabItem();}
    ImGui::EndTabBar();}
    ImGui::PopItemWidth();ImGui::End();ImGui::PopStyleVar(2);
    const bool blocked=ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId|ImGuiPopupFlags_AnyPopupLevel)||io.WantTextInput||ImGui::IsAnyItemActive();
    input_.SetRegion({static_cast<LONG>(min.x),static_cast<LONG>(min.y),static_cast<LONG>(max.x),static_cast<LONG>(max.y)},hovered,blocked);
}
}
