#include "UI/ImagePointLights.h"
namespace isr {
bool CanEditImagePoints(const RelightingSession& s){
    return s.CanEditPointLights();
}
PointCamera ImagePointCamera(const SourceObservation& source){
    const auto& k=source.analysisMaps->metadata["camera"]["intrinsicsNormalized"];PointCamera c;c.fx=k[0];c.fy=k[4];c.cx=k[2];c.cy=k[5];return c;
}
std::optional<ImagePointLight> ImagePointAt(const RelightingSession& s,float u,float v){
    if(!CanEditImagePoints(s)||!std::isfinite(u+v)||u<0||u>=1||v<0||v>=1)return {};
    const auto& maps=*s.Source()->analysisMaps;const auto& position=*maps.maps[3].image;
    const auto x=std::min(uint32_t(u*position.width),position.width-1),y=std::min(uint32_t(v*position.height),position.height-1);const auto index=size_t(y)*position.width+x;
    // Never interpolate across a hole or fabricate the depth of an unobserved pixel.
    if(!maps.maps[4].image->UintAt(index))return {};
    const float depth=position.FloatAt(index,2);if(!std::isfinite(depth)||depth<=0)return {};
    const auto p=UnprojectPoint(ImagePointCamera(*s.Source()),u,v,std::max(.01f,depth*.85f));
    ImagePointLight light;light.position={p.x,p.y,p.z};light.range=std::clamp(depth*.6f,.01f,10000.f);
    light.intensity=std::clamp(depth*depth*.04f,.01f,10000.f);
    for(const auto& old:s.pointLights)light.id=std::max(light.id,old.id+1);
    if(!light.Valid())return {};return light;
}
bool DrawImagePointAddButton(RelightingSession& s){
    ImGui::BeginDisabled(!CanEditImagePoints(s)||s.pointLights.size()>=MaxImagePointLights);
    const bool clicked=ImGui::Button("添加点光源###AddImagePoint");
    if(clicked){s.pointEdit.placing=true;s.pointEdit.showHelpers=true;s.pointEdit.dragging.reset();s.imageView=ImageDebugView::Relighted;}
    ImGui::EndDisabled();return clicked;
}
void DrawImagePointPanel(RelightingSession& s){
    if(!ImGui::CollapsingHeader("图像点光源###ImagePoints",ImGuiTreeNodeFlags_DefaultOpen))return;
    const bool available=CanEditImagePoints(s);DrawImagePointAddButton(s);
    if(!available)ImGui::TextWrapped("需要有效的点位置、几何法线、相机内参与来源光照。请先加载带分析数据的重建包。");
    if(s.pointLights.size()>=MaxImagePointLights)ImGui::TextWrapped("图像模式最多支持 4 个点光源。");
    if(s.pointEdit.placing){ImGui::TextWrapped("在目标画面点击放置；无效几何区域不能放置。Esc 取消。");if(ImGui::Button("取消放置###CancelImagePoint"))s.pointEdit.Cancel();}
    for(size_t i=0;i<s.pointLights.size();++i){const auto label="点光源 "+std::to_string(s.pointLights[i].id)+"###imagePoint"+std::to_string(s.pointLights[i].id);
        if(ImGui::Selectable(label.c_str(),s.pointEdit.selected==i)){s.pointEdit.selected=i;s.pointEdit.showHelpers=true;}}
    if(s.pointEdit.selected&&*s.pointEdit.selected<s.pointLights.size()){
        auto& p=s.pointLights[*s.pointEdit.selected];const auto previous=p;ImGui::PushID("imagePointInspector");
        ImGui::Checkbox("启用点光源###Enabled",&p.enabled);ImGui::ColorEdit3("颜色###Color",p.color.data(),ImGuiColorEditFlags_Float);
        ImGui::DragFloat("强度###Intensity",&p.intensity,.02f,0,10000,"%.3f",ImGuiSliderFlags_AlwaysClamp);
        ImGui::DragFloat("范围###Range",&p.range,.02f,.01f,10000,"%.3f",ImGuiSliderFlags_AlwaysClamp);
        if(available){const auto camera=ImagePointCamera(*s.Source());auto uv=ProjectPoint(camera,{p.position[0],p.position[1],p.position[2]});float depth=p.position[2];
            if(ImGui::DragFloat("来源相机深度###Depth",&depth,.02f,.01f,100000,"%.3f",ImGuiSliderFlags_AlwaysClamp)&&uv){auto point=UnprojectPoint(camera,uv->x,uv->y,depth);p.position={point.x,point.y,point.z};}}
        if(!p.Valid())p=previous; // Typed values must obey the same finite bounds as recipe loading.
        ImGui::Text("来源坐标：%.3f，%.3f，%.3f",p.position[0],p.position[1],p.position[2]);
        ImGui::Checkbox("显示位置与范围###Helpers",&s.pointEdit.showHelpers);
        if(ImGui::Button("删除点光源###Delete")){s.pointLights.erase(s.pointLights.begin()+*s.pointEdit.selected);s.pointEdit.selected.reset();s.pointEdit.Cancel();}
        ImGui::PopID();
    }
    if(!s.pointLights.empty()&&ImGui::Button("清空图像点光源###ClearImagePoints")){s.pointLights.clear();s.pointEdit={};}
    ImGui::TextWrapped("仅修改目标漫反射照明，受置信度和保护蒙版约束；不改变原图或三维灯光。深度和范围采用重建尺度。暂不增加点光源阴影与镜面高光；可随配方保存。");
}
bool DrawImagePointHandles(RelightingSession& s,const PointCanvas& canvas,ImVec2 clipMin,ImVec2 clipMax,bool hovered){
    if(ImGui::IsKeyPressed(ImGuiKey_Escape))s.pointEdit.Cancel();
    if(!CanEditImagePoints(s))return false;auto& edit=s.pointEdit;auto& io=ImGui::GetIO();
    const float u=(io.MousePos.x-canvas.origin.x)/canvas.size.x,v=(io.MousePos.y-canvas.origin.y)/canvas.size.y;
    // Keep submitting handles while they own hover. The overlapping canvas item
    // then reports !hovered; using that flag would alternate ownership each frame.
    const bool inside=ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)&&io.MousePos.x>=clipMin.x&&io.MousePos.x<clipMax.x&&io.MousePos.y>=clipMin.y&&io.MousePos.y<clipMax.y;
    const auto camera=ImagePointCamera(*s.Source());bool owns=edit.placing;
    if(edit.placing){
        if(inside&&hovered&&!PointInputBlocked()&&!ImGui::IsAnyItemActive()&&ImGui::IsMouseClicked(ImGuiMouseButton_Left)){
            if(auto point=ImagePointAt(s,u,v);point&&s.pointLights.size()<MaxImagePointLights){s.pointLights.push_back(*point);edit.selected=s.pointLights.size()-1;edit.placing=false;}}
    }else if(edit.showHelpers&&edit.selected){
        for(size_t i=0;i<s.pointLights.size();++i){auto& p=s.pointLights[i];ImGui::PushID(int(p.id));
            if(!PointInputBlocked()&&(inside||edit.dragging==i)){
                const auto handle=PointHandle(camera,canvas,{p.position[0],p.position[1],p.position[2]},edit.dragging==i);owns|=handle.owns;
                if(handle.activated){edit.selected=i;edit.dragging=i;edit.dragDepth=p.position[2];}
                if(handle.active&&edit.dragging==i&&!handle.activated){
                    const auto moved=UnprojectPoint(camera,u,v,edit.dragDepth);auto next=p;next.position={moved.x,moved.y,moved.z};if(next.Valid())p=next;}
            }ImGui::PopID();
        }
    }
    if(!io.MouseDown[0])edit.dragging.reset();return owns;
}
void DrawImagePointOverlays(const RelightingSession& s,const PointCanvas& canvas,bool hovered){
    if(!CanEditImagePoints(s))return;const auto camera=ImagePointCamera(*s.Source());const auto& edit=s.pointEdit;
    if(edit.placing&&hovered){const auto mouse=ImGui::GetIO().MousePos;
        if(auto point=ImagePointAt(s,(mouse.x-canvas.origin.x)/canvas.size.x,(mouse.y-canvas.origin.y)/canvas.size.y))
            DrawPointOverlay(camera,canvas,{point->position[0],point->position[1],point->position[2]},point->range,IM_COL32(255,210,120,180),true);
        else ImGui::GetWindowDrawList()->AddText({canvas.origin.x+8,canvas.origin.y+30},IM_COL32(255,180,150,255),"当前位置没有有效几何，请换一处放置。");
    }
    if(edit.showHelpers&&edit.selected)for(size_t i=0;i<s.pointLights.size();++i){const auto& p=s.pointLights[i];
        DrawPointOverlay(camera,canvas,{p.position[0],p.position[1],p.position[2]},p.range,ImGui::ColorConvertFloat4ToU32({p.color[0],p.color[1],p.color[2],p.enabled?1.f:.35f}),edit.selected==i);}
}
}
