#include "UI/ViewportTools.h"
#include "Core/Log.h"
#include <algorithm>
#include <cmath>
namespace isr {
using namespace DirectX;
namespace {
bool Inside(ImVec2 p,ImVec2 a,ImVec2 b){return p.x>=a.x&&p.x<b.x&&p.y>=a.y&&p.y<b.y;}
void Outline(const Scene& scene,const SceneSelection& selection,const SceneEditState& edit,ImVec2 a,ImVec2 b){
    if(selection.kind!=SelectionKind::Entity||selection.index>=scene.entities.size())return;
    const auto bounds=edit.SelectionBounds(scene,selection.index);if(!bounds)return;
    ImVec2 points[8];bool valid[8]{};const auto vp=scene.camera.View()*scene.camera.Projection();
    for(unsigned i=0;i<8;++i){const auto& box=*bounds;XMFLOAT4 clip;
        XMStoreFloat4(&clip,XMVector4Transform(XMVectorSet(i&1?box.maximum.x:box.minimum.x,i&2?box.maximum.y:box.minimum.y,i&4?box.maximum.z:box.minimum.z,1),vp));
        valid[i]=clip.w>1e-5f&&clip.z>=0&&clip.z<=clip.w;
        if(valid[i])points[i]={a.x+(clip.x/clip.w+1)*.5f*(b.x-a.x),a.y+(1-clip.y/clip.w)*.5f*(b.y-a.y)};
    }
    auto* draw=ImGui::GetWindowDrawList();draw->PushClipRect(a,b,true);
    for(unsigned i=0;i<8;++i)for(unsigned axis=1;axis<=4;axis*=2)if(!(i&axis)&&valid[i]&&valid[i|axis])draw->AddLine(points[i],points[i|axis],IM_COL32(255,190,65,230),1.5f);
    const auto label="Selected: "+scene.entities[selection.index].name;
    draw->AddText({a.x+9,b.y-ImGui::GetTextLineHeight()-8},IM_COL32(255,210,115,255),label.c_str());draw->PopClipRect();
}
}
bool ViewportTools::Draw(Scene& scene,SceneSelection& selection,SceneEditState& edit,ImVec2 a,ImVec2 b,bool imageHovered){
    auto& io=ImGui::GetIO();std::optional<size_t> light;
    if(selection.kind==SelectionKind::Light&&selection.index<scene.lights.size()&&scene.lights[selection.index].type==LightType::Directional)light=selection.index;
    if(!light)for(size_t i=0;i<scene.lights.size();++i)if(scene.lights[i].type==LightType::Directional){light=i;break;}
    const bool show=light&&b.x-a.x>190&&b.y-a.y>170;
    const ImVec2 minimum{b.x-147,a.y+10},maximum{b.x-10,a.y+155};
    if(imageHovered&&!io.MouseDown[1]&&!io.KeyAlt&&!ImGui::IsAnyItemActive()&&ImGui::IsMouseClicked(ImGuiMouseButton_Left)&&(!show||!Inside(io.MousePos,minimum,maximum))){
        if(auto entity=edit.Pick(scene,(io.MousePos.x-a.x)/(b.x-a.x),(io.MousePos.y-a.y)/(b.y-a.y))){
            selection={SelectionKind::Entity,*entity};Log("Viewport selected: "+scene.entities[*entity].name);}
    }
    Outline(scene,selection,edit,a,b);
    if(!show){dragLight_.reset();return false;}
    const auto cursor=ImGui::GetCursorScreenPos();ImGui::SetCursorScreenPos(minimum);
    ImGui::InvisibleButton("##sunDirection",{maximum.x-minimum.x,maximum.y-minimum.y},ImGuiButtonFlags_MouseButtonLeft);
    const bool hovered=ImGui::IsItemHovered(),active=ImGui::IsItemActive();
    if(ImGui::IsItemActivated()){dragLight_=light;selection={SelectionKind::Light,*light};}
    if(active&&!ImGui::IsItemActivated()&&dragLight_&&*dragLight_<scene.lights.size()&&(io.MouseDelta.x||io.MouseDelta.y)){
        auto& sun=scene.lights[*dragLight_];sun.direction=DragSunDirection(sun.direction,scene.camera,io.MouseDelta.x,io.MouseDelta.y);
    }
    if(ImGui::IsItemDeactivated()&&dragLight_){Log("Sun gizmo edited directional light "+std::to_string(*dragLight_));dragLight_.reset();}
    if(hovered)ImGui::SetTooltip("Drag with left mouse to rotate the sun relative to the camera.\nThe arrow points toward the sun; Inspector Direction is light travel.\nLighting changes are visible in Final mode.");
    auto* draw=ImGui::GetWindowDrawList();draw->PushClipRect(a,b,true);
    draw->AddRectFilled(minimum,maximum,IM_COL32(12,19,28,225),8);
    draw->AddText({minimum.x+10,minimum.y+7},IM_COL32(245,214,145,255),"SUN / drag LMB");
    const ImVec2 center{(minimum.x+maximum.x)*.5f,minimum.y+77};constexpr float radius=41;
    draw->AddCircleFilled(center,radius,IM_COL32(32,45,60,255),48);draw->AddCircle(center,radius,IM_COL32(112,139,159,255),48);
    draw->AddLine({center.x-radius,center.y},{center.x+radius,center.y},IM_COL32(69,89,105,255));
    draw->AddLine({center.x,center.y-radius},{center.x,center.y+radius},IM_COL32(69,89,105,255));
    XMFLOAT3 direction;XMStoreFloat3(&direction,XMVector3TransformNormal(-XMVector3Normalize(XMLoadFloat3(&scene.lights[*light].direction)),scene.camera.View()));
    const ImVec2 tip{center.x+direction.x*radius,center.y-direction.y*radius};
    const auto color=IM_COL32(255,199,78,255);draw->AddLine(center,tip,color,2.5f);
    draw->AddCircleFilled(tip,6,color,16);if(direction.z>0)draw->AddCircle(tip,9,IM_COL32(240,149,68,255),20);
    draw->AddText({minimum.x+10,minimum.y+126},IM_COL32(164,179,190,255),direction.z>0?"Sun: into view":"Sun: toward eye");
    draw->PopClipRect();ImGui::SetCursorScreenPos(cursor);ImGui::Dummy({0,0});return hovered||active;
}
}
