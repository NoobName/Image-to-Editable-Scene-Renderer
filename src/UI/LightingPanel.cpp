#include "UI/ChineseText.h"
#include "UI/LightingPanel.h"
#include "UI/ImagePointLights.h"
#include <imgui.h>
namespace isr {
namespace {
void Controls(LightingParameters& p){
    ImGui::DragFloat3("光线传播方向###Travel direction",p.direction.data(),.01f,-1,1,"%.3f");
    ImGui::ColorEdit3("直射光颜色###Direct color",p.directColor.data());ImGui::SliderFloat("直射光增益###Direct gain",&p.directIntensity,0,8);
    ImGui::ColorEdit3("环境光颜色###Ambient color",p.ambientColor.data());ImGui::SliderFloat("环境补光###Environment fill",&p.ambientIntensity,0,8);
}
void Arrow(const LightingParameters& p){
    const auto start=ImGui::GetCursorScreenPos();ImGui::Dummy({90,50});const ImVec2 center{start.x+45,start.y+25};
    const ImVec2 end{center.x-p.direction[0]*35,center.y+p.direction[1]*22}; // Screen Y down; camera Y up.
    auto* draw=ImGui::GetWindowDrawList();draw->AddCircle(center,22,IM_COL32(120,120,120,255));
    draw->AddLine(center,end,IM_COL32(255,220,80,255),3);draw->AddCircleFilled(end,3,IM_COL32(255,220,80,255));
    ImGui::SameLine();ImGui::Text("朝向光源 XY\n传播方向 Z = %.3f",p.direction[2]);
}
}
bool DrawApplySourceButton(LightingSession& state){
    if(ImGui::Button("应用来源校准###Apply source calibration"))return state.ApplySource();return false;
}
bool DrawResetTargetButton(LightingSession& state){
    if(!ImGui::Button("重置目标方向光与环境光###Reset target to source"))return false;state.target=state.source;state.targetGlobalGain=1;return true;
}
bool DrawCastShadowToggle(RelightingParameters& parameters){return ImGui::Checkbox("成对投射阴影###Paired cast shadows",&parameters.castShadows);}
bool DrawImageFogToggle(ImageFogParameters& parameters){return ImGui::Checkbox("添加图像气氛###Add image atmosphere",&parameters.enabled);}
int DrawLightingPanel(RelightingSession& session,bool busy){
    DrawImagePointPanel(session);
    if(ImGui::CollapsingHeader("额外图像气氛###Additional image atmosphere",session.fog.enabled?ImGuiTreeNodeFlags_DefaultOpen:0)){
        auto& fog=session.fog;DrawImageFogToggle(fog);
        ImGui::SliderFloat("额外雾密度###Additional density",&fog.density,0,10,"%.4f",ImGuiSliderFlags_AlwaysClamp);
        ImGui::ColorEdit3("空气光（线性 RGB）###Airlight (linear RGB)",fog.airlight.data(),ImGuiColorEditFlags_Float);
        ImGui::Checkbox("允许相对 / 合成尺度###Allow relative / synthetic scale",&fog.allowRelativeScale);
        if(ImGui::Button("重置额外气氛###Reset additional atmosphere"))fog={};
        if(session.Source()&&session.Source()->analysisMaps){const auto& maps=session.Source()->analysisMaps->metadata;
            ImGui::TextWrapped("几何尺度：%s。密度单位是几何单位的倒数。",ChineseText(maps["camera"]["scaleType"].get<std::string>()).c_str());}
        else ImGui::TextWrapped("不可用：缺少已验证的分析几何。");
        ImGui::TextWrapped("来源额外雾固定为 0，仅添加薄雾，不移除原图已有雾。已知天空、无效或未知尺度及保护区保持不变。相对尺度需显式启用。原生上限为 8M 像素。效果在显示曝光前应用，独立于光照强度。");
    }
    ImGui::SeparatorText("来源 / 目标光照");int action=0;
    if(ImGui::CollapsingHeader("离线分析 / 重新构建###Offline analysis / rebuild")){
    const bool hasMaps=session.Source()&&session.Source()->analysisMaps;
    ImGui::BeginDisabled(busy||!hasMaps);if(ImGui::Button("拟合已保存观测###Fit saved observations"))action=1;ImGui::EndDisabled();
    ImGui::BeginDisabled(busy||!hasMaps);if(ImGui::Button("拟合本征辅助漫反射###Fit intrinsic-assisted diffuse"))action=4;ImGui::EndDisabled();
    ImGui::BeginDisabled(busy||!hasMaps);if(ImGui::Button("估计已保存图像的本征分解###Estimate saved image intrinsics"))action=3;ImGui::EndDisabled();
    ImGui::BeginDisabled(busy||!hasMaps);if(ImGui::Button("分析旧阴影支持度###Analyze old-shadow support"))action=5;ImGui::EndDisabled();
    }
    auto& state=session.lighting;
    if(!state.available){ImGui::TextWrapped("不可用：缺少 lighting/lighting.json。场景默认灯光不是估计结果。");return action;}
    const auto& data=session.Source()->lighting->metadata;const auto& fit=data["fit"];
    if(!state.cacheValid)ImGui::TextWrapped("以下是历史拟合指标；当前校准尚未评估。");
    ImGui::TextWrapped("%s | %s",ChineseText(fit["backend"].get<std::string>()).c_str(),ChineseText(fit["status"].get<std::string>()).c_str());
    ImGui::Text("置信度 %.3f（启发式）",fit["confidence"].get<double>());
    ImGui::Text("支持比例 %.1f%%",100*fit["validFraction"].get<double>());
    ImGui::Text("均方根误差 %.4f → %.4f",fit["beforeRMSE"].get<double>(),fit["afterRMSE"].get<double>());
    ImGui::Text("代理值中位数 %.5f",fit["normalization"].get<double>());
    if(data.contains("assistance")&&ImGui::CollapsingHeader("分析来源 / 误差###Analysis provenance / errors")){const auto& a=data["assistance"];
        ImGui::TextWrapped("本征拟合：%s",ChineseText(a["reason"].get<std::string>()).c_str());
        ImGui::Text("全局尺度 %.5f / 颜色增益 1",a["calibrationScale"].get<double>());
        ImGui::Text("各自观测均方根误差：基准 %.4f / 辅助 %.4f",a["baselineFit"]["afterRMSE"].get<double>(),a["candidateFit"]["afterRMSE"].get<double>());
        ImGui::TextWrapped("两者使用不同观测。应比较 lighting.json 中统一尺度下的逐区域误差，不能只比较这两个均方根误差。");
    }
    ImGui::TextWrapped("采用相对尺度；曝光为 0、反照率增益为 1。单位不是勒克斯或坎德拉。");
    if(ImGui::CollapsingHeader("拟合局限###Fit limitations"))for(const auto& reason:fit["reasons"])ImGui::TextWrapped("%s",ChineseText(reason.get<std::string>()).c_str());
    if(state.manualSource)ImGui::TextWrapped("来源：手工校准，导出前仅在当前会话中生效。");
    else ImGui::TextWrapped("来源：%s",ChineseText(data["sourceLighting"]["provenance"].get<std::string>()).c_str());
    Arrow(state.source);
    if(!state.cacheValid)ImGui::TextWrapped("旧明暗 / 残差不可用：来源已改变。请导出校准以重建数据。");
    if(ImGui::CollapsingHeader("来源光照校准###Source calibration")){
        ImGui::PushID("source");Controls(state.draft);
        DrawApplySourceButton(state);
        if(ImGui::Button("放弃来源草稿###Discard source draft"))state.draft=state.source;
        ImGui::BeginDisabled(busy||!state.manualSource);if(ImGui::Button("导出校准后来源###Export calibrated source"))action=2;ImGui::EndDisabled();
        ImGui::TextWrapped("应用会使旧明暗数据失效，但目标光照不变。不接受零方向向量。");ImGui::PopID();
    }
    if(ImGui::CollapsingHeader("目标光照 / 重光照###Target lighting / relighting",ImGuiTreeNodeFlags_DefaultOpen)){
        ImGui::PushID("target");
        ImGui::SliderFloat("全局强度###Global intensity",&state.targetGlobalGain,0,4,"%.2f",ImGuiSliderFlags_AlwaysClamp);
        if(ImGui::IsItemHovered())ImGui::SetTooltip("同时乘以下方目标方向光、环境光和图像点光源的基础增益。来源光照和显示曝光保持不变。");
        Controls(state.target);Arrow(state.target);
        if(ImGui::Button("将来源复制到目标###Copy source to target")){state.target=state.source;state.targetGlobalGain=1;}
        DrawResetTargetButton(state);
        ImGui::SliderFloat("重光照强度###Relighting strength",&session.relighting.strength,0,1);
        int color=int(session.relighting.colorMode);const char* modes[]{"亮度响应（保留颜色）","有界彩色照明"};
        if(ImGui::Combo("颜色响应###Color response",&color,modes,2))session.relighting.colorMode=static_cast<RatioColorMode>(color);
        ImGui::Checkbox("使用稳定性控制###Use stability controls",&session.relighting.stability);
        ImGui::Checkbox("本征漫反射保护###Intrinsic diffuse protection",&session.relighting.intrinsicProtection);
        ImGui::Checkbox("保守高光处理###Conservative specular handling",&session.relighting.specularEnabled);
        ImGui::SliderFloat("高光编辑强度###Specular edit strength",&session.relighting.specularStrength,0,1);
        ImGui::SliderFloat("目标粗糙度响应###Target roughness response",&session.relighting.specularRoughnessScale,.5f,2);
        DrawCastShadowToggle(session.relighting);
        ImGui::SliderFloat("投射阴影强度###Cast shadow strength",&session.relighting.shadowStrength,0,1);
        ImGui::SliderFloat("阴影置信度倍率###Shadow confidence scale",&session.relighting.shadowConfidence,0,1);
        ImGui::SliderInt("图像阴影 PCF 半径###Image shadow PCF radius",&session.relighting.shadowPcfRadius,0,2);
        ImGui::SliderFloat("图像阴影深度偏移###Image shadow depth bias",&session.relighting.shadowBias,0,.01f,"%.5f");
        ImGui::SliderFloat("图像阴影法线偏移###Image shadow normal bias",&session.relighting.shadowNormalBias,0,.1f,"%.4f");
        ImGui::TextWrapped("仅使用固定的来源观测几何。移除旧阴影需要照片与几何依据一致；隐藏或屏外遮挡物、深黑和未知区域仍不受支持。关闭后恢复前阶段合成。");
        if(ImGui::Button("重置高光响应###Reset specular response")){session.relighting.specularStrength=.25f;session.relighting.specularRoughnessScale=1;}
        ImGui::TextWrapped("仅支持非金属的方向光高光。金属、玻璃、自发光及未知区域保持受保护；关闭后恢复仅漫反射的结果。来源重新校准后，旧分解在重新拟合前不可用。");
        if(ImGui::Button("保守预设###Conservative preset"))session.relighting.Conservative();
        if(ImGui::Button("重置响应控制###Reset response controls"))session.relighting={};
        if(ImGui::TreeNode("保护范围 / 材质响应###Protection range / material response")){
            ImGui::SliderFloat("最小比率###Minimum ratio",&session.relighting.minRatio,.1f,1);
            ImGui::SliderFloat("最大比率###Maximum ratio",&session.relighting.maxRatio,1,8);
            ImGui::SliderFloat("色度变化上限###Chroma limit",&session.relighting.chromaLimit,1,2);
            ImGui::TextWrapped("材质响应使用现有粗糙度缩放和有界高光强度。环境补光提供环境光，不是完整反射。额外图像气氛使用独立控件。");ImGui::TreePop();}
        ImGui::Text("稳定项 ε=%.3f，比率=[%.2f,%.2f]",session.relighting.epsilon,session.relighting.minRatio,session.relighting.maxRatio);
        ImGui::TextWrapped("启发式权重相互独立，有效性是硬性条件。导入 PNG 中的白色保留原图。投射阴影需要已验证的阴影依据；未观测到的阴影与反射仍会保留。");
        ImGui::TextWrapped("目标编辑不改变来源光照、拟合结果或三维场景灯光。");ImGui::PopID();
    }
    return action;
}
}
