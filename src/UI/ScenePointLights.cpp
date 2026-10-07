#include "UI/ScenePointLights.h"
#include "UI/PointLightGizmo.h"
namespace isr {
void ScenePointAddButton(Scene& scene,PointLightInteraction& state){
    ImGui::BeginDisabled(scene.lights.size()>=MaxSceneLights);
    if(ImGui::Button("添加点光源###AddScenePoint")){state.placing=true;state.showHelpers=true;state.dragging.reset();}
    ImGui::EndDisabled();if(scene.lights.size()>=MaxSceneLights)ImGui::TextWrapped("三维场景最多支持 8 个光源（含方向光）。");
    if(state.placing){ImGui::TextWrapped("点击视口放置点光源，Esc 取消。");if(ImGui::Button("取消放置###CancelScenePoint"))state.Cancel();}
}
bool DrawScenePointLights(Scene& scene,SceneSelection& selection,SceneEditState& edit,PointLightInteraction& state,ImVec2 a,ImVec2 b,bool hovered){
    if(ImGui::IsKeyPressed(ImGuiKey_Escape))state.Cancel();
    const PointCanvas canvas{a,{b.x-a.x,b.y-a.y}};const auto camera=PointCameraFor(scene.camera);auto& io=ImGui::GetIO();
    const bool selected=selection.kind==SelectionKind::Light&&selection.index<scene.lights.size()&&scene.lights[selection.index].type==LightType::Point;
    if(!selected&&!state.placing){state.dragging.reset();return false;}
    auto* draw=ImGui::GetWindowDrawList();draw->PushClipRect(a,b,true);bool owns=state.placing;
    const float u=(io.MousePos.x-a.x)/canvas.size.x,v=(io.MousePos.y-a.y)/canvas.size.y;
    if(state.placing&&hovered&&!PointInputBlocked()&&!ImGui::IsAnyItemActive()){
        float depth=5;DirectX::XMFLOAT3 hit;
        if(edit.Pick(scene,u,v,&hit))depth=std::max(.05f,InPointCamera(camera,hit).z*.85f);
        auto p=UnprojectPoint(camera,u,v,depth);DrawPointOverlay(camera,canvas,p,std::max(.2f,depth*.6f),IM_COL32(255,210,120,200),true);
        if(ImGui::IsMouseClicked(ImGuiMouseButton_Left)&&scene.lights.size()<MaxSceneLights){
            Light light;light.type=LightType::Point;light.position=p;light.range=std::max(.2f,depth*.6f);light.intensity=std::max(1.f,depth*depth*.1f);
            scene.lights.push_back(light);selection={SelectionKind::Light,scene.lights.size()-1};state.placing=false;state.selected=selection.index;
        }
    }else if(selected&&state.showHelpers){
        for(size_t i=0;i<scene.lights.size();++i){auto& light=scene.lights[i];if(light.type!=LightType::Point)continue;ImGui::PushID(int(i));
            const bool usable=!PointInputBlocked()&&hovered;
            if(usable||state.dragging==i){const auto handle=PointHandle(camera,canvas,light.position,state.dragging==i);owns|=handle.owns;
                if(handle.activated){selection={SelectionKind::Light,i};state.dragging=i;state.dragDepth=InPointCamera(camera,light.position).z;}
                if(handle.active&&state.dragging==i&&!handle.activated)light.position=UnprojectPoint(camera,u,v,std::max(.01f,state.dragDepth));}
            DrawPointOverlay(camera,canvas,light.position,light.range,ImGui::ColorConvertFloat4ToU32({light.color.x,light.color.y,light.color.z,light.enabled?1.f:.35f}),i==selection.index);
            ImGui::PopID();
        }
    }
    if(!io.MouseDown[0])state.dragging.reset();draw->PopClipRect();return owns;
}
void ScenePointInspector(Scene& scene,SceneSelection& selection,PointLightInteraction& state){
    if(selection.index>=scene.lights.size()||scene.lights[selection.index].type!=LightType::Point)return;
    auto& light=scene.lights[selection.index];const auto camera=PointCameraFor(scene.camera);auto uv=ProjectPoint(camera,light.position);
    float depth=InPointCamera(camera,light.position).z;
    ImGui::BeginDisabled(!uv);if(ImGui::DragFloat("相机深度###ScenePointDepth",&depth,.05f,.01f,100000,"%.3f",ImGuiSliderFlags_AlwaysClamp)&&uv)light.position=UnprojectPoint(camera,uv->x,uv->y,depth);ImGui::EndDisabled();
    ImGui::Checkbox("启用点光源###ScenePointEnabled",&light.enabled);ImGui::Checkbox("显示位置与范围###ScenePointHelpers",&state.showHelpers);
    ImGui::TextWrapped("世界坐标光源；仅影响三维场景。虚线是球形范围，点光源暂不投射阴影。修改在当前会话中生效。");
    if(ImGui::Button("删除点光源###DeleteScenePoint")){scene.lights.erase(scene.lights.begin()+selection.index);selection={SelectionKind::Camera,0};state.Cancel();}
}
}
