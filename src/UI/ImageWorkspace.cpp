#include "UI/ImageWorkspace.h"
#include "UI/ImagePointLights.h"
#include <algorithm>
#include <cmath>
#include <cstring>
namespace isr {
ImageRect DisplayImageRect(uint32_t sw,uint32_t sh,float width,float height,const ImageDisplayState& s){
    const float scale=std::min(width/sw,height/sh)*s.zoom,w=sw*scale,h=sh*scale;
    return {(width-w)*.5f+s.panX*w,(height-h)*.5f+s.panY*h,w,h,scale};
}
namespace {
bool LightSpaceView(ImageDebugView view){return view==ImageDebugView::CastOldMap||view==ImageDebugView::CastNewMap;}
bool ViewItem(RelightingSession& s,int i){if(ImGui::Selectable(ImageDebugNames[i],int(s.imageView)==i)){s.imageView=ImageDebugView(i);return true;}return false;}
void Views(RelightingSession& s,const char* group,int first,int last){if(ImGui::BeginMenu(group)){for(int i=first;i<=last;++i)ViewItem(s,i);ImGui::EndMenu();}}
}
void DrawImageToolbar(RelightingSession& s){
    ImGui::SetNextItemWidth(std::max(90.f,ImGui::GetContentRegionAvail().x-55));
    if(ImGui::BeginCombo("##imageDebugView",ImageDebugNames[int(s.imageView)])){
        const int main[]{0,2,3,8,15,19,20,24,25};for(int i:main)ViewItem(s,i);
        ImGui::Separator();Views(s,"坐标 / 几何",1,14);Views(s,"原始光照拟合 / 残差",15,23);
        Views(s,"置信度 / 比率 / 保护",26,37);Views(s,"本征分解 / 残差",38,45);
        Views(s,"高光（有界方向光）",46,54);Views(s,"旧阴影依据",55,63);Views(s,"成对投射阴影",64,73);
        Views(s,"额外图像气氛",74,77);
        ImGui::EndCombo();}
    ImGui::SameLine();if(ImGui::Button("适应窗口###Fit"))s.display.Fit();
    const char* compare[]{"单图","并排对比","擦除对比"};ImGui::SetNextItemWidth(115);
    ImGui::BeginDisabled(LightSpaceView(s.imageView));
    ImGui::Combo("##comparison",&s.display.comparison,compare,3);
    ImGui::EndDisabled();
    if(ImGui::GetContentRegionAvail().x>235)ImGui::SameLine();
    ImGui::TextDisabled("适应比例的 %.0f%% | %s",s.display.zoom*100,LightSpaceView(s.imageView)?"光源空间贴图":"固定相机");
}
void DrawOriginalReference(const RelightingSession& s,ImTextureID texture,ImTextureID reference,ImTextureID residual){
    ImGui::TextUnformatted("原图 / 来源");
    if(s.CanDisplayImage()){
        const auto& a=*s.Source()->anchor;const float w=std::max(1.f,ImGui::GetContentRegionAvail().x);
        const auto rect=FitSourceImage(a.sourceSize[0],a.sourceSize[1],uint32_t(w),uint32_t(std::max(1.f,w*1.2f)));
        ImGui::SetCursorPosX(ImGui::GetCursorPosX()+rect.x);ImGui::Image(texture,{rect.width,rect.height});
        ImGui::Text("%u x %u",a.sourceSize[0],a.sourceSize[1]);
        ImGui::TextWrapped("原图像素保持固定，三维编辑不会改变这张图像。");
    }else ImGui::TextWrapped("没有经过验证的原图资产。");
    ImGui::SeparatorText("参考图");
    if(s.reference.analysis){const auto& data=*s.reference.analysis;const auto& image=*data.previews[s.reference.showResidual?1:0];const float w=std::max(1.f,ImGui::GetContentRegionAvail().x);
        const auto rect=FitSourceImage(image.width,image.height,uint32_t(w),uint32_t(std::max(1.f,w*1.1f)));
        ImGui::Image(s.reference.showResidual?residual:reference,{rect.width,rect.height});
        ImGui::TextWrapped("%s | 置信度 %.3f",s.reference.showResidual?"参考图拟合残差":"参考图分析图像",data.confidence);
        ImGui::TextWrapped("光照建议采用相机相对坐标，原图像素保持固定。");
    }else ImGui::TextWrapped("尚未分析参考图。请在“参考图”页检查光照建议后再应用。");
}
bool DrawImageSun(RelightingSession& s,ImVec2 origin){
    const auto cursor=ImGui::GetCursorScreenPos();ImGui::SetCursorScreenPos(origin);
    ImGui::BeginDisabled(!s.lighting.available);ImGui::InvisibleButton("##imageTargetSun",{80,80});
    const bool active=ImGui::IsItemActive(),hover=ImGui::IsItemHovered();
    if(active&&!ImGui::IsItemActivated()){
        auto& d=s.lighting.target.direction;const auto delta=ImGui::GetIO().MouseDelta;
        const float len=std::sqrt(d[0]*d[0]+d[1]*d[1]+d[2]*d[2]);
        const float yaw=std::atan2(d[0],d[2])+delta.x*.012f;
        const float pitch=std::clamp(std::asin(std::clamp(d[1]/std::max(len,1e-6f),-1.f,1.f))-delta.y*.012f,-1.55f,1.55f);
        d={std::sin(yaw)*std::cos(pitch),std::sin(pitch),std::cos(yaw)*std::cos(pitch)};
    }
    auto* draw=ImGui::GetWindowDrawList();const ImVec2 c{origin.x+40,origin.y+40};
    draw->AddCircleFilled(c,37,IM_COL32(18,24,32,225));draw->AddCircle(c,36,IM_COL32(170,170,170,255));
    auto arrow=[&](const LightingParameters& p,ImU32 color){ImVec2 end{c.x-p.direction[0]*30,c.y+p.direction[1]*30};draw->AddLine(c,end,color,2);draw->AddCircleFilled(end,3,color);};
    arrow(s.lighting.source,IM_COL32(60,190,255,255));arrow(s.lighting.target,IM_COL32(255,210,60,255));
    if(hover)ImGui::SetTooltip("在固定来源相机空间中拖动目标太阳。\n蓝色表示原光照，黄色表示目标光照；通过偏航和俯仰保留 Z 方向。");
    ImGui::EndDisabled();ImGui::SetCursorScreenPos(cursor);ImGui::Dummy({0,0});return active||hover;
}
void DrawImageCanvas(RelightingSession& s,ImTextureID original,ImTextureID rendered,ImVec2 size){
    const ImVec2 origin=ImGui::GetCursorScreenPos();ImGui::SetNextItemAllowOverlap();ImGui::InvisibleButton("##imageCanvas",size,(s.pointEdit.placing?0:ImGuiButtonFlags_MouseButtonLeft)|ImGuiButtonFlags_MouseButtonMiddle);
    const bool hovered=ImGui::IsItemHovered(),canvasActive=ImGui::IsItemActive();
    if(!s.CanDisplayImage())return;
    auto& state=s.display;const auto& a=*s.Source()->anchor;const auto& io=ImGui::GetIO();auto* draw=ImGui::GetWindowDrawList();
    // Shadow depth maps have their own square light projection, not source image coordinates.
    const bool lightSpace=LightSpaceView(s.imageView);const int comparison=lightSpace?0:state.comparison;
    const auto sw=lightSpace?1u:a.sourceSize[0],sh=lightSpace?1u:a.sourceSize[1];
    const ImVec2 end{origin.x+size.x,origin.y+size.y};draw->PushClipRect(origin,end,true);draw->AddRectFilled(origin,end,IM_COL32(6,8,10,255));
    const bool showSun=s.lighting.available&&size.x>=190&&size.y>=160;
    const bool sun=showSun&&DrawImageSun(s,{end.x-88,origin.y+8});
    // Popup/text/slider focus owns input. Middle drag pans; LMB selects or drags the wipe divider.
    const bool blocked=ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId|ImGuiPopupFlags_AnyPopupLevel)||io.WantTextInput||sun||(!canvasActive&&ImGui::IsAnyItemActive());
    const float pane=comparison==1?size.x*.5f:size.x;
    const auto pointRect=DisplayImageRect(sw,sh,pane,size.y,state);const float pointOffset=comparison==1?pane:0;
    const PointCanvas pointCanvas{{origin.x+pointOffset+pointRect.x,origin.y+pointRect.y},{pointRect.width,pointRect.height}};
    const ImVec2 pointClip{origin.x+(comparison==1?pane:comparison==2?size.x*state.wipe:0),origin.y};
    const bool pointTool=!lightSpace&&!sun&&DrawImagePointHandles(s,pointCanvas,pointClip,end,hovered);
    const float offset=comparison==1&&io.MousePos.x>=origin.x+pane?pane:0;
    auto rect=DisplayImageRect(sw,sh,pane,size.y,state);
    const float localX=io.MousePos.x-origin.x-offset,localY=io.MousePos.y-origin.y;
    if(hovered&&!blocked&&!pointTool&&io.MouseWheel!=0){
        const float u=(localX-rect.x)/rect.width,v=(localY-rect.y)/rect.height;
        state.zoom=std::clamp(state.zoom*std::pow(1.2f,io.MouseWheel),.25f,16.f);
        auto next=DisplayImageRect(sw,sh,pane,size.y,state);
        state.panX+=(localX-next.x-u*next.width)/next.width;state.panY+=(localY-next.y-v*next.height)/next.height;
    }
    if(canvasActive&&!blocked&&!pointTool&&ImGui::IsMouseDragging(ImGuiMouseButton_Middle,0)){
        state.panX=std::clamp(state.panX+io.MouseDelta.x/rect.width,-2.f,2.f);state.panY=std::clamp(state.panY+io.MouseDelta.y/rect.height,-2.f,2.f);}
    if(canvasActive&&!blocked&&!pointTool&&comparison==2&&ImGui::IsMouseDown(ImGuiMouseButton_Left))state.wipe=std::clamp((io.MousePos.x-origin.x)/size.x,0.f,1.f);
    if(hovered&&!blocked&&!pointTool&&!lightSpace&&comparison!=2&&ImGui::IsMouseClicked(ImGuiMouseButton_Left)){
        s.protection.selected=SourceLabel(*s.Source(),(localX-rect.x)/rect.width,(localY-rect.y)/rect.height);
        if(s.protection.selected){auto found=s.protection.weights.find(*s.protection.selected);s.protection.draft=found==s.protection.weights.end()?1:found->second;}}
    // The render target already has a fit rectangle. Crop its letterbox before applying the UI display transform.
    const auto fit=FitSourceImage(sw,sh,uint32_t(size.x),uint32_t(size.y));
    const ImVec2 uv0{fit.x/size.x,fit.y/size.y},uv1{(fit.x+fit.width)/size.x,(fit.y+fit.height)/size.y};
    auto image=[&](float x,float width,bool source){auto r=DisplayImageRect(sw,sh,width,size.y,state);
        ImVec2 lo{origin.x+x+r.x,origin.y+r.y},hi{lo.x+r.width,lo.y+r.height};
        draw->PushClipRect({origin.x+x,origin.y},{origin.x+x+width,end.y},true);
        const bool native=source||s.imageView==ImageDebugView::Original;
        draw->AddImage(native?original:rendered,lo,hi,native?ImVec2{0,0}:uv0,native?ImVec2{1,1}:uv1);draw->PopClipRect();};
    if(comparison==1){image(0,pane,true);image(pane,pane,false);draw->AddLine({origin.x+pane,origin.y},{origin.x+pane,end.y},IM_COL32_WHITE);}
    else{image(0,size.x,false);if(comparison==2){draw->PushClipRect(origin,{origin.x+size.x*state.wipe,end.y},true);image(0,size.x,true);draw->PopClipRect();
        draw->AddLine({origin.x+size.x*state.wipe,origin.y},{origin.x+size.x*state.wipe,end.y},IM_COL32_WHITE,2);}}
    draw->AddText({origin.x+6,origin.y+5},IM_COL32(70,200,255,255),lightSpace?"光源空间 / 不支持原图选区":(comparison?"原图":"主视图"));
    if(comparison)draw->AddText({origin.x+(comparison==1?pane:size.x*state.wipe)+6,origin.y+5},IM_COL32(255,210,60,255),"目标 / 调试");
    if(!lightSpace){draw->PushClipRect(pointClip,end,true);DrawImagePointOverlays(s,pointCanvas,hovered&&io.MousePos.x>=pointClip.x);draw->PopClipRect();}
    // Draw the gizmo after images without adding a second interactive item.
    if(showSun){const ImVec2 c{end.x-48,origin.y+48};draw->AddCircleFilled(c,37,IM_COL32(18,24,32,225));
        for(int i=0;i<2;++i){const auto& p=i?s.lighting.target:s.lighting.source;ImVec2 q{c.x-p.direction[0]*30,c.y+p.direction[1]*30};
            draw->AddLine(c,q,i?IM_COL32(255,210,60,255):IM_COL32(60,190,255,255),2);draw->AddCircleFilled(q,3,IM_COL32_WHITE);}}
    if(hovered&&!sun&&!lightSpace){rect=DisplayImageRect(sw,sh,pane,size.y,state);
        const float u=(localX-rect.x)/rect.width,v=(localY-rect.y)/rect.height;
        if(u>=0&&v>=0&&u<1&&v<1){ImGui::BeginTooltip();ImGui::Text("原图像素 %u，%u",uint32_t(u*a.sourceSize[0]),uint32_t(v*a.sourceSize[1]));
            if(auto label=SourceLabel(*s.Source(),u,v))ImGui::Text("稳定蒙版标签 %u",*label);ImGui::EndTooltip();}}
    draw->PopClipRect();
}
void DrawImageProtection(RelightingSession& s){
    ImGui::SeparatorText("图像编辑（独立于三维）");
    ImGui::TextWrapped("滚轮缩放，中键平移，“适应窗口”重置显示。左键选择区域；擦除对比时左键移动分隔线。黄色太阳仅编辑目标光照。");
    ImGui::SliderFloat("显示曝光（EV）###Exposure (display EV)",&s.display.exposure,-4,4,"%.2f");
    ImGui::Checkbox("低置信度叠加显示###Low confidence overlay",&s.display.lowConfidence);
    if(s.display.lowConfidence)ImGui::SliderFloat("置信度阈值###Confidence threshold",&s.display.confidenceThreshold,0,1);
    ImGui::TextWrapped("曝光和红色叠加仅用于重光照结果；数值调试视图与原图像素保持不变。");
    ImGui::SeparatorText("区域保护");
    if(!s.Source()||!s.Source()->analysisMaps||!s.Source()->analysisMaps->maps[5].image){ImGui::TextWrapped("不可用：缺少稳定区域标签图。");return;}
    if(s.protection.selected){const auto label=*s.protection.selected;ImGui::Text("标签：%u",label);
        bool named=false;for(const auto& r:s.Source()->regions)if(r.label==label){ImGui::TextWrapped("%s | 标识：%s",r.name.c_str(),r.id.c_str());named=true;break;}
        if(!named)ImGui::TextWrapped("标识：sourceId + 标签 %u（区域未命名）",label);
        ImGui::SliderFloat("保护权重###Protection weight",&s.protection.draft,0,1);
        DrawApplyRegionProtection(s);
    }else ImGui::TextWrapped("在单图或并排对比中选择区域。即使网格有孔洞，也可通过原图标签选择。");
    if(ImGui::Button("清除本次区域保护###Clear session protection"))s.protection.Clear();
    ImGui::TextWrapped("白色表示保留原图，仅影响重光照权重。导入的保护蒙版继续生效；成功加载新包后，本次区域编辑重置。");
}
bool DrawApplyRegionProtection(RelightingSession& s){
    if(ImGui::Button("应用区域保护###Apply region protection")&&s.protection.selected){s.protection.Apply(*s.protection.selected,s.protection.draft);return true;}return false;
}
}
