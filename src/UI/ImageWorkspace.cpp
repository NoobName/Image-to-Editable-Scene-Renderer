#include "UI/ImageWorkspace.h"
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
        ImGui::Separator();Views(s,"Coordinates / geometry",1,14);Views(s,"Original fit / residual",15,23);
        Views(s,"Confidence / ratio / protection",26,37);Views(s,"Intrinsic / residual",38,45);
        Views(s,"Specular (bounded directional)",46,54);Views(s,"Old shadow evidence",55,63);Views(s,"Paired cast shadows",64,73);
        Views(s,"Additional image atmosphere",74,77);
        ImGui::EndCombo();}
    ImGui::SameLine();if(ImGui::Button("Fit"))s.display.Fit();
    const char* compare[]{"Single","Side by side","Wipe"};ImGui::SetNextItemWidth(115);
    ImGui::BeginDisabled(LightSpaceView(s.imageView));
    ImGui::Combo("##comparison",&s.display.comparison,compare,3);
    ImGui::EndDisabled();
    if(ImGui::GetContentRegionAvail().x>235)ImGui::SameLine();
    ImGui::TextDisabled("%.0f%% of fit | %s",s.display.zoom*100,LightSpaceView(s.imageView)?"light-space map":"fixed camera");
}
void DrawOriginalReference(const RelightingSession& s,ImTextureID texture,ImTextureID reference,ImTextureID residual){
    ImGui::TextUnformatted("ORIGINAL / SOURCE");
    if(s.CanDisplayImage()){
        const auto& a=*s.Source()->anchor;const float w=std::max(1.f,ImGui::GetContentRegionAvail().x);
        const auto rect=FitSourceImage(a.sourceSize[0],a.sourceSize[1],uint32_t(w),uint32_t(std::max(1.f,w*1.2f)));
        ImGui::SetCursorPosX(ImGui::GetCursorPosX()+rect.x);ImGui::Image(texture,{rect.width,rect.height});
        ImGui::Text("%u x %u",a.sourceSize[0],a.sourceSize[1]);
        ImGui::TextWrapped("Fixed source pixels. 3D edits do not alter this image.");
    }else ImGui::TextWrapped("No validated source anchor.");
    ImGui::SeparatorText("REFERENCE");
    if(s.reference.analysis){const auto& data=*s.reference.analysis;const auto& image=*data.previews[s.reference.showResidual?1:0];const float w=std::max(1.f,ImGui::GetContentRegionAvail().x);
        const auto rect=FitSourceImage(image.width,image.height,uint32_t(w),uint32_t(std::max(1.f,w*1.1f)));
        ImGui::Image(s.reference.showResidual?residual:reference,{rect.width,rect.height});
        ImGui::TextWrapped("%s | confidence %.3f",s.reference.showResidual?"Reference fit residual":"Reference analysis RGB",data.confidence);
        ImGui::TextWrapped("Camera-relative lighting proposal. Source pixels stay fixed.");
    }else ImGui::TextWrapped("No reference analyzed. Open the Reference tab to inspect a proposal before applying.");
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
    if(hover)ImGui::SetTooltip("Drag TARGET sun in fixed source-camera space.\nBlue = source; yellow = target. Z is retained through yaw/pitch.");
    ImGui::EndDisabled();ImGui::SetCursorScreenPos(cursor);ImGui::Dummy({0,0});return active||hover;
}
void DrawImageCanvas(RelightingSession& s,ImTextureID original,ImTextureID rendered,ImVec2 size){
    const ImVec2 origin=ImGui::GetCursorScreenPos();ImGui::SetNextItemAllowOverlap();ImGui::InvisibleButton("##imageCanvas",size,ImGuiButtonFlags_MouseButtonLeft|ImGuiButtonFlags_MouseButtonMiddle);
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
    const float offset=comparison==1&&io.MousePos.x>=origin.x+pane?pane:0;
    auto rect=DisplayImageRect(sw,sh,pane,size.y,state);
    const float localX=io.MousePos.x-origin.x-offset,localY=io.MousePos.y-origin.y;
    if(hovered&&!blocked&&io.MouseWheel!=0){
        const float u=(localX-rect.x)/rect.width,v=(localY-rect.y)/rect.height;
        state.zoom=std::clamp(state.zoom*std::pow(1.2f,io.MouseWheel),.25f,16.f);
        auto next=DisplayImageRect(sw,sh,pane,size.y,state);
        state.panX+=(localX-next.x-u*next.width)/next.width;state.panY+=(localY-next.y-v*next.height)/next.height;
    }
    if(canvasActive&&!blocked&&ImGui::IsMouseDragging(ImGuiMouseButton_Middle,0)){
        state.panX=std::clamp(state.panX+io.MouseDelta.x/rect.width,-2.f,2.f);state.panY=std::clamp(state.panY+io.MouseDelta.y/rect.height,-2.f,2.f);}
    if(canvasActive&&!blocked&&comparison==2&&ImGui::IsMouseDown(ImGuiMouseButton_Left))state.wipe=std::clamp((io.MousePos.x-origin.x)/size.x,0.f,1.f);
    if(hovered&&!blocked&&!lightSpace&&comparison!=2&&ImGui::IsMouseClicked(ImGuiMouseButton_Left)){
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
    draw->AddText({origin.x+6,origin.y+5},IM_COL32(70,200,255,255),lightSpace?"LIGHT SPACE / NO SOURCE PICK":(comparison?"SOURCE":"MAIN VIEW"));
    if(comparison)draw->AddText({origin.x+(comparison==1?pane:size.x*state.wipe)+6,origin.y+5},IM_COL32(255,210,60,255),"TARGET / DEBUG");
    // Draw the gizmo after images without adding a second interactive item.
    if(showSun){const ImVec2 c{end.x-48,origin.y+48};draw->AddCircleFilled(c,37,IM_COL32(18,24,32,225));
        for(int i=0;i<2;++i){const auto& p=i?s.lighting.target:s.lighting.source;ImVec2 q{c.x-p.direction[0]*30,c.y+p.direction[1]*30};
            draw->AddLine(c,q,i?IM_COL32(255,210,60,255):IM_COL32(60,190,255,255),2);draw->AddCircleFilled(q,3,IM_COL32_WHITE);}}
    if(hovered&&!sun&&!lightSpace){rect=DisplayImageRect(sw,sh,pane,size.y,state);
        const float u=(localX-rect.x)/rect.width,v=(localY-rect.y)/rect.height;
        if(u>=0&&v>=0&&u<1&&v<1){ImGui::BeginTooltip();ImGui::Text("Source texel %u, %u",uint32_t(u*a.sourceSize[0]),uint32_t(v*a.sourceSize[1]));
            if(auto label=SourceLabel(*s.Source(),u,v))ImGui::Text("Stable mask label %u",*label);ImGui::EndTooltip();}}
    draw->PopClipRect();
}
void DrawImageProtection(RelightingSession& s){
    ImGui::SeparatorText("Image editing (independent of 3D)");
    ImGui::TextWrapped("Wheel zoom | MMB pan | Fit reset. LMB selects labels; in Wipe, LMB moves divider. Yellow sun edits target only.");
    ImGui::SliderFloat("Exposure (display EV)",&s.display.exposure,-4,4,"%.2f");
    ImGui::Checkbox("Low confidence overlay",&s.display.lowConfidence);
    if(s.display.lowConfidence)ImGui::SliderFloat("Confidence threshold",&s.display.confidenceThreshold,0,1);
    ImGui::TextWrapped("Exposure / red overlay apply only to Relighted. Numerical debug views and source pixels stay unchanged.");
    ImGui::SeparatorText("Region protection");
    if(!s.Source()||!s.Source()->analysisMaps||!s.Source()->analysisMaps->maps[5].image){ImGui::TextWrapped("Unavailable: stable label map absent.");return;}
    if(s.protection.selected){const auto label=*s.protection.selected;ImGui::Text("Label: %u",label);
        bool named=false;for(const auto& r:s.Source()->regions)if(r.label==label){ImGui::TextWrapped("%s | ID: %s",r.name.c_str(),r.id.c_str());named=true;break;}
        if(!named)ImGui::TextWrapped("ID: sourceId + label:%u (no named object)",label);
        ImGui::SliderFloat("Protection weight",&s.protection.draft,0,1);
        DrawApplyRegionProtection(s);
    }else ImGui::TextWrapped("Select a region in Single or Side by side. Selection reads source labels even where geometry has holes.");
    if(ImGui::Button("Clear session protection"))s.protection.Clear();
    ImGui::TextWrapped("White = preserve source. Applied to relighting weights only. Imported protection remains; session edits reset on successful package load.");
}
bool DrawApplyRegionProtection(RelightingSession& s){
    if(ImGui::Button("Apply region protection")&&s.protection.selected){s.protection.Apply(*s.protection.selected,s.protection.draft);return true;}return false;
}
}
