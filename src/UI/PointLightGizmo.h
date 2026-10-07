#pragma once
#include "Scene/PointLightMath.h"
#include <imgui.h>
#include <algorithm>
#include <array>
namespace isr {
struct PointCanvas {ImVec2 origin,size;};
inline bool PointInside(ImVec2 p,const PointCanvas& r){return p.x>=r.origin.x&&p.y>=r.origin.y&&p.x<r.origin.x+r.size.x&&p.y<r.origin.y+r.size.y;}
inline ImVec2 PointPixel(DirectX::XMFLOAT2 uv,const PointCanvas& r){return {r.origin.x+uv.x*r.size.x,r.origin.y+uv.y*r.size.y};}
inline void DrawPointOverlay(const PointCamera& camera,const PointCanvas& canvas,DirectX::XMFLOAT3 p,float radius,ImU32 color,bool range){
    auto* draw=ImGui::GetWindowDrawList();
    if(range)for(unsigned axis=0;axis<3;++axis)for(unsigned i=0;i<96;i+=2){
        auto sample=[&](unsigned step){const float angle=DirectX::XM_2PI*step/96;std::array<float,3> xyz{p.x,p.y,p.z};
            xyz[(axis+1)%3]+=std::cos(angle)*radius;xyz[(axis+2)%3]+=std::sin(angle)*radius;return ProjectPoint(camera,{xyz[0],xyz[1],xyz[2]});};
        const auto a=sample(i),b=sample(i+1);if(a&&b)draw->AddLine(PointPixel(*a,canvas),PointPixel(*b,canvas),IM_COL32(240,240,235,140),1.25f);
    }
    if(auto uv=ProjectPoint(camera,p)){const auto c=PointPixel(*uv,canvas);draw->AddCircleFilled(c,8,color);draw->AddCircle(c,9,IM_COL32_WHITE,32,2);}
}
struct PointHandleState {bool owns=false,activated=false,active=false;};
inline PointHandleState PointHandle(const PointCamera& camera,const PointCanvas& canvas,DirectX::XMFLOAT3 p,bool dragging){
    const auto uv=ProjectPoint(camera,p);if(!uv)return {};const auto center=PointPixel(*uv,canvas);
    if(!dragging&&!PointInside(center,canvas))return {};
    const auto cursor=ImGui::GetCursorScreenPos();ImGui::SetCursorScreenPos({center.x-12,center.y-12});
    ImGui::InvisibleButton("##pointHandle",{24,24});
    const PointHandleState result{ImGui::IsItemHovered()||ImGui::IsItemActive(),ImGui::IsItemActivated(),ImGui::IsItemActive()};
    if(ImGui::IsItemHovered())ImGui::SetTooltip("左键拖动位置圆点；右侧调整深度与范围。\n虚线为三维球形影响范围，不代表阴影边界。");
    // Restoring an absolute cursor requires a submitted layout item in ImGui.
    // Capture active-item state first: Dummy becomes the next LastItem.
    ImGui::SetCursorScreenPos(cursor);ImGui::Dummy({0,0});return result;
}
inline bool PointInputBlocked(){return ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId|ImGuiPopupFlags_AnyPopupLevel)||ImGui::GetIO().WantTextInput;}
}
