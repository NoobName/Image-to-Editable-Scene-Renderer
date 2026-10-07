#include "UI/ReferencePanel.h"
#include "UI/ChineseText.h"
#include "Assets/AssetIO.h"
#include <imgui.h>
namespace isr {
bool DrawApplyReferenceButton(RelightingSession& session){return ImGui::Button("应用目标建议###Apply target proposal")&&ApplyReferenceProposal(session);}
bool DrawResetReferenceButton(RelightingSession& session){return ImGui::Button("恢复参考图应用前的目标###Restore target before reference")&&ResetReferenceTarget(session);}
bool DrawOptimizeTargetButton(const RelightingSession& session,ReferenceActions& actions){
    if(!ImGui::Button("优化当前目标###Optimize current target")||!session.reference.analysis)return false;
    actions.input=session.reference.analysis->root;actions.request=ReferenceAction::Optimize;return true;
}
void ReferencePanel::Draw(RelightingSession& session,bool otherBusy){
    if(!actions_)return;auto& a=*actions_;
    try{if(auto selected=picker_.Poll();selected&&!selected->empty()){
        const auto text=PathUtf8(*selected);strncpy_s(path_.data(),path_.size(),text.c_str(),_TRUNCATE);
        if(loadProposal_){a.input=*selected;a.request=ReferenceAction::Load;}
    }}catch(const std::exception& e){a.status=e.what();}
    ImGui::TextWrapped("参考光照 | 相机相对坐标建议，不迁移 RGB 图像内容");
    ImGui::BeginDisabled(a.busy||picker_.Busy()||otherBusy||!session.CanDisplayImage());
    ImGui::InputText("图像 / 场景包路径###Image/package path",path_.data(),path_.size());
    if(ImGui::Button("浏览参考图…###Browse reference...")){loadProposal_=false;picker_.Reference(owner_);}
    ImGui::SameLine();if(ImGui::Button("打开建议…###Open proposal...")){loadProposal_=true;picker_.Recipe(owner_,false);}
    int relation=a.relation=="same-scene"?0:1;const char* relations[]{"同一场景（手工声明）","不同内容 / 风格"};
    if(ImGui::Combo("参考关系###Relation",&relation,relations,2))a.relation=relation==0?"same-scene":"different-content";
    ImGui::Checkbox("使用已配置的缓存后端###Use configured cached backends",&a.useConfiguredBackends);
    ImGui::Checkbox("允许带说明的回退###Allow labelled fallback",&a.allowFallback);
    ImGui::TextWrapped("已保存的场景包会复用观测数据。原始图像使用“文件”中的重建设置，仅离线运行。回退结果不代表恢复出了光照方向。");
    if(ImGui::Button("分析参考图###Analyze reference")&&path_[0]){a.input=std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(path_.data())));a.request=ReferenceAction::Analyze;}
    ImGui::EndDisabled();if(a.busy&&ImGui::Button("取消参考分析###Cancel reference analysis"))a.cancel=true;
    ImGui::TextWrapped("%s",ChineseText(a.status).c_str());if(!a.log.empty()&&ImGui::Button("复制参考分析日志路径###Copy reference log path"))ImGui::SetClipboardText(PathUtf8(a.log).c_str());
    if(!session.reference.analysis)return;
    const auto& data=*session.reference.analysis;const auto& j=data.metadata;const auto& p=j.at("proposal");
    ImGui::SeparatorText("应用前检查");ImGui::Checkbox("显示参考图残差###Show reference residual",&session.reference.showResidual);
    ImGui::Text("置信度 %.3f | %s",data.confidence,data.canApply?"建议可用":"不可用");
    ImGui::TextWrapped("%s",ChineseText(p.at("relation").get_ref<const std::string&>()).c_str());
    ImGui::TextWrapped("参考相机到来源相机：按约定对齐 XYZ 轴。共同世界坐标中的太阳位置未知。");
    const auto& current=session.lighting.EffectiveTarget();const auto& target=data.target;
    ImGui::Text("传播方向：%.3f %.3f %.3f",target.direction[0],target.direction[1],target.direction[2]);
    ImGui::Text("直射光 %.3f → %.3f",current.directIntensity,target.directIntensity);ImGui::Text("环境光 %.3f → %.3f",current.ambientIntensity,target.ambientIntensity);
    ImGui::Text("拟合均方根误差 %.4f → %.4f",j.at("fit").at("beforeRMSE").get<double>(),j.at("fit").at("afterRMSE").get<double>());
    ImGui::TextWrapped("应用将改变新明暗、成对高光 / 阴影和比率。来源、曝光及画面调整保持固定。");
    auto* draw=ImGui::GetWindowDrawList();const auto cursor=ImGui::GetCursorScreenPos();const ImVec2 c{cursor.x+50,cursor.y+35};draw->AddCircle(c,29,IM_COL32(160,160,160,255));
    draw->AddLine(c,{c.x-current.direction[0]*26,c.y+current.direction[1]*26},IM_COL32(50,190,255,255),2);
    draw->AddLine(c,{c.x-target.direction[0]*26,c.y+target.direction[1]*26},data.canApply?IM_COL32(255,210,60,255):IM_COL32(110,110,110,255),2);ImGui::Dummy({100,70});
    ImGui::TextWrapped("XY 箭头指向光源；传播方向 Z 值见上方。蓝色为当前目标，黄色为建议。");
    if(data.confidence<.5f)ImGui::TextWrapped("低置信度：请检查残差及原因；有足够依据时，可在应用后手工修正方向。");
    for(const auto& reason:p.at("reasons"))ImGui::TextWrapped("%s",ChineseText(reason.get_ref<const std::string&>()).c_str());
    ImGui::TextWrapped("无法推断：绝对辐射亮度、共同世界方向、HDR 环境与相对曝光。");
    ImGui::SeparatorText("有界目标优化");
    ImGui::Checkbox("已确认像素配准对齐###Registered pixel alignment confirmed",&a.registered);
    ImGui::SliderInt("迭代上限###Iteration limit",&a.iterations,1,200);
    ImGui::TextWrapped("来源、法线、反照率与曝光保持固定。同场景需要像素配准；不同内容仅使用照明统计。运行期间的修改会使结果失效。");
    if(!session.pointLights.empty())ImGui::TextWrapped("自动优化暂不包含点光源；请先清空图像点光源。参考光照建议仍可手工应用。");
    ImGui::BeginDisabled(a.busy||otherBusy||!data.canApply||!data.optimization.is_null()||!session.pointLights.empty());
    DrawOptimizeTargetButton(session,a);
    ImGui::EndDisabled();
    if(!data.optimization.is_null()){
        const auto& o=data.optimization;ImGui::TextWrapped("%s: %s",ChineseText(o.at("status").get_ref<const std::string&>()).c_str(),ChineseText(o.at("reason").get_ref<const std::string&>()).c_str());
        ImGui::Text("损失 %.6g → %.6g",o.at("initialLoss").get<double>(),o.at("bestLoss").get<double>());
        std::vector<float> loss;for(const auto& h:o.at("history"))loss.push_back(h.at("bestLoss").get<float>());
        if(!loss.empty())ImGui::PlotLines("固定目标损失###Fixed loss",loss.data(),int(loss.size()),0,nullptr,0,FLT_MAX,{0,90});
        ImGui::Text("DX12 已验证：%s | 误差 %.3g",session.reference.gpuVerified?"是":"否",session.reference.gpuError);
        ImGui::TextWrapped("CPU 评估器覆盖漫反射新明暗。轨迹、蒙版和残差见 debug/optimization.png；完整高光、阴影与比率结果通过原尺寸导出检查。");
    }
    ImGui::BeginDisabled(a.busy||otherBusy||!data.canApply);
    try{DrawApplyReferenceButton(session);if(session.reference.applied)DrawResetReferenceButton(session);}
    catch(const std::exception& e){a.status=e.what();}ImGui::EndDisabled();
}
}
