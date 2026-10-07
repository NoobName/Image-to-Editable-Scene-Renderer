#include "UI/LookDevelopmentUI.h"
#include "UI/ImageWorkspace.h"
#include <imgui.h>
#include <algorithm>
#include <cmath>
namespace isr {
namespace {
constexpr auto PanelFlags=ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoCollapse|
    ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoBringToFrontOnFocus;
void Place(float x,float y,float w,float h){ImGui::SetNextWindowPos({x,y});ImGui::SetNextWindowSize({w,h});}
}
void LookDevelopmentUI::DrawPanels(Scene& scene,RenderSettings& settings,EnvironmentManager& environment,RelightingSession& session){
    const auto& io=ImGui::GetIO();const float w=io.DisplaySize.x,h=io.DisplaySize.y;
    const float top=reconstruction_.Draw(),height=std::max(1.0f,h-top);
    const float left=std::min(220.0f,w*0.20f),right=std::min(355.0f,w*0.34f),middle=std::max(1.0f,w-left-right);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,0);ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize,1);
    const auto previousSelection=selection_;
    Place(0,top,left,height);ImGui::Begin(session.Mode()==WorkMode::ImageRelighting?"原图 / 参考图###Original / Reference":"场景###Scene",nullptr,PanelFlags);
    if(session.Mode()==WorkMode::ImageRelighting)DrawOriginalReference(session,static_cast<ImTextureID>(heap_.Gpu(SourceSlot).ptr),static_cast<ImTextureID>(heap_.Gpu(ReferenceSlot).ptr),static_cast<ImTextureID>(heap_.Gpu(ReferenceResidualSlot).ptr));
    else {ScenePointAddButton(scene,viewportTools_.points);DrawSceneHierarchy(scene,selection_);}ImGui::End();
    Place(left,top,middle,height);ImGui::Begin("主视口###Viewport",nullptr,PanelFlags|ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
    modeChanged_=DrawWorkModeControls(session);
    const bool imageMode=session.Mode()==WorkMode::ImageRelighting;
    bool frameAll=false,frameSelected=false;
    if(!imageMode){
    int mode=static_cast<int>(settings.mode);ImGui::SetNextItemWidth(std::min(230.0f,middle*0.45f));
    if(ImGui::Combo("##renderMode",&mode,RenderModeNames,RenderModeCount))settings.mode=static_cast<RenderMode>(mode);
    if(ImGui::IsItemHovered())ImGui::SetTooltip("几何上的原图：将处理后照片投影到可编辑网格，与独立原图视图不同。\n估计视图显示未乘材质因子的贴图；估计法线为切线空间法线。");
    ImGui::SameLine();frameAll=ImGui::Button("聚焦全部###Frame all");
    ImGui::SameLine();frameSelected=ImGui::Button("聚焦所选###Frame selected");
    }else{
        DrawImageToolbar(session);
    }
    const auto available=ImGui::GetContentRegionAvail();
    viewportWidth_=static_cast<uint32_t>(std::max(1.0f,std::floor(available.x)));
    viewportHeight_=static_cast<uint32_t>(std::max(1.0f,std::floor(available.y-ImGui::GetTextLineHeightWithSpacing()*2)));
    if(!imageMode){scene.camera.SetAspect(float(viewportWidth_)/float(viewportHeight_));
        if(frameAll)FrameScene(scene);else if(frameSelected)FrameScene(scene,&selection_);}
    const auto min=ImGui::GetCursorScreenPos();const ImVec2 max{min.x+viewportWidth_,min.y+viewportHeight_};
    if(imageMode)DrawImageCanvas(session,static_cast<ImTextureID>(heap_.Gpu(SourceSlot).ptr),static_cast<ImTextureID>(heap_.Gpu(ViewportSlot).ptr),{float(viewportWidth_),float(viewportHeight_)});
    else ImGui::Image(static_cast<ImTextureID>(heap_.Gpu(ViewportSlot).ptr),{float(viewportWidth_),float(viewportHeight_)});
    const bool hovered=ImGui::IsMouseHoveringRect(min,max)&&ImGui::IsWindowHovered();
    toolMouse_=false;
    if(!imageMode)toolMouse_=viewportTools_.Draw(scene,selection_,edit_,min,max,hovered);
    else viewportTools_.Reset();
    ImGui::TextDisabled(imageMode?"二维图像显示 | 三维相机已暂停":"左键选择 | 右键转向 | WASDQE 移动 | 滚轮前后移动");
    ImGui::TextDisabled("%u × %u | %.1f 帧/秒",viewportWidth_,viewportHeight_,io.Framerate);ImGui::End();
    Place(left+middle,top,right,height);ImGui::Begin("属性面板###Inspector",nullptr,PanelFlags);
    ImGui::PushItemWidth(std::max(80.0f,right*0.52f));
    const bool selectionChanged=selection_.kind!=previousSelection.kind||selection_.index!=previousSelection.index;
    if(imageMode){
        if(ImGui::BeginTabBar("ImageInspector")){
            if(ImGui::BeginTabItem("光照###Lighting")){reconstruction_.DrawLighting(session);ImGui::EndTabItem();}
            if(ImGui::BeginTabItem("图像 / 保护###Image / Protection")){DrawImageProtection(session);ImGui::EndTabItem();}
            if(ImGui::BeginTabItem("配方 / 导出###Recipe / Export",nullptr,recipe_.TakeFocus()?ImGuiTabItemFlags_SetSelected:0)){recipe_.Draw(session.CanDisplayImage(),reconstruction_.Busy());ImGui::EndTabItem();}
            if(ImGui::BeginTabItem("参考图###Reference",nullptr,showReference_?ImGuiTabItemFlags_SetSelected:0)){referencePanel_.Draw(session,reconstruction_.Busy());ImGui::EndTabItem();}showReference_=false;
            if(ImGui::BeginTabItem("数据来源###Sources")){DrawSourceInformation(session,viewportWidth_,viewportHeight_);ImGui::EndTabItem();}
            ImGui::EndTabBar();}}
    else if(ImGui::BeginTabBar("InspectorTabs")){
    if(ImGui::BeginTabItem("物体###Object",nullptr,selectionChanged?ImGuiTabItemFlags_SetSelected:0)){
    switch(selection_.kind){
    case SelectionKind::Entity:DrawEntityInspector(scene,selection_.index,edit_,settings);break;
    case SelectionKind::Camera:DrawCameraInspector(scene.camera);break;
    case SelectionKind::Light:if(selection_.index<scene.lights.size()){DrawLightInspector(scene.lights[selection_.index],settings);ScenePointInspector(scene,selection_,viewportTools_.points);}break;
    case SelectionKind::Environment:environmentPanel_.Draw(settings,environment);break;
    }
    ImGui::EndTabItem();}
    if(ImGui::BeginTabItem("画面调整###Look")){DrawDisplaySettings(settings);ImGui::EndTabItem();}
    if(ImGui::BeginTabItem("配方 / 导出###Recipe / Export")){recipe_.Draw(session.CanDisplayImage(),reconstruction_.Busy());ImGui::EndTabItem();}
    ImGui::EndTabBar();}
    ImGui::PopItemWidth();ImGui::End();ImGui::PopStyleVar(2);
    reconstruction_.DrawWindows();
    const bool blocked=imageMode||modeChanged_||toolMouse_||ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId|ImGuiPopupFlags_AnyPopupLevel)||io.WantTextInput||ImGui::IsAnyItemActive()||(!hovered&&io.WantCaptureMouse);
    input_.SetRegion({static_cast<LONG>(min.x),static_cast<LONG>(min.y),static_cast<LONG>(max.x),static_cast<LONG>(max.y)},hovered,blocked);
}
}
