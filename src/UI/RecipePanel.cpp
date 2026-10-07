#include "UI/RecipePanel.h"
#include "UI/ChineseText.h"
#include <imgui.h>
#include <shellapi.h>
#include <algorithm>
namespace isr {
bool DrawRefinementButton(RecipeActions& actions){
    if(ImGui::Button("运行可选神经优化###Run optional neural refinement")){actions.request=RecipeAction::Refine;return true;}
    return false;
}
void RecipePanel::Draw(bool available,bool reconstructionBusy){
    if(!actions_)return;auto& a=*actions_;
    try{if(auto path=dialog_.Poll();path&&!path->empty()){a.path=*path;a.request=pending_;}}
    catch(const std::exception& e){a.status=e.what();}
    ImGui::TextWrapped("配方 v3 可保存图像点光源；无点光源时仍保存 v2。兼容 v1 / v2，不覆盖来源场景包。");
    ImGui::BeginDisabled(a.busy||dialog_.Busy()||reconstructionBusy);
    if(ImGui::Button("打开配方…###Open recipe...")){pending_=RecipeAction::Open;dialog_.Recipe(owner_,false);}
    ImGui::BeginDisabled(!available);
    if(ImGui::Button("另存新配方…###Save new recipe...")){pending_=RecipeAction::SaveNew;dialog_.Recipe(owner_,true);}
    if(ImGui::Button("覆盖配方…###Replace recipe...")){pending_=RecipeAction::Replace;dialog_.Recipe(owner_,true);}
    ImGui::TextWrapped("仅“覆盖配方”会替换已有文件；另存拒绝重名。原尺寸导出上限：单边 8192、总计 8 Mi 像素，不会自动缩小。");
    ImGui::InputText("导出目录###Export directory",exportPath_.data(),exportPath_.size());
    if(ImGui::Button("导出原尺寸 PNG 与浮点 DDS###Export native PNG + float DDS")&&exportPath_[0]){a.path=std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(exportPath_.data())));a.request=RecipeAction::Export;}
    ImGui::EndDisabled();ImGui::EndDisabled();
    if(a.busy&&ImGui::Button("取消配方加载###Cancel recipe load"))a.cancel=true;
    ImGui::Separator();ImGui::TextWrapped("%s",ChineseText(a.status).c_str());
    ImGui::TextWrapped("PNG 是显示用 sRGB；DDS 是显示用线性数据，不是恢复的 HDR 辐射亮度。导出不包含界面叠加、适应窗口、平移和缩放。");
    ImGui::Separator();
    if(std::exchange(openRefinement_,false))ImGui::SetNextItemOpen(true);
    if(ImGui::CollapsingHeader("可选离线神经优化###Optional offline neural refinement")){
        ImGui::TextWrapped("在物理结果之后生成研究候选，原图与物理结果保持不变。权重采用 CC BY-NC 许可，需要可选依赖。");
        ImGui::BeginDisabled(a.refinementBusy||a.busy||reconstructionBusy||!available);
        ImGui::SliderFloat("神经优化强度###Refinement strength",&a.refinementStrength,0,1,"%.3f");
        ImGui::InputInt("随机种子###Refinement seed",&a.refinementSeed);a.refinementSeed=std::max(0,a.refinementSeed);
        DrawRefinementButton(a);ImGui::EndDisabled();
        if(a.refinementBusy&&ImGui::Button("取消神经优化###Cancel refinement"))a.cancelRefinement=true;
        ImGui::TextWrapped("%s",ChineseText(a.refinementStatus).c_str());
        if(!a.refinementReport.empty()&&ImGui::Button("对比原图 / 物理结果 / 优化结果###Compare Original / Physics / Refined")){
            const auto result=ShellExecuteW(owner_,L"open",a.refinementReport.c_str(),nullptr,nullptr,SW_SHOWNORMAL);
            if(reinterpret_cast<INT_PTR>(result)<=32)a.refinementStatus="无法在默认浏览器中打开本地对比报告。";
        }
        ImGui::TextWrapped("对比会打开本地离线报告，包含差异、引导、保护和拒绝区域。主视口保留物理结果，候选不代表实测几何。");
    }
}
}
