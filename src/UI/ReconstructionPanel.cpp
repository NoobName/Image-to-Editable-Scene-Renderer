#include "UI/ReconstructionPanel.h"
#include "Assets/AssetIO.h"
#include "UI/LightingPanel.h"
#include "UI/ChineseText.h"
#include <imgui.h>
#include <algorithm>
namespace isr {
void ReconstructionPanel::DrawLighting(RelightingSession& session){
    const auto action=DrawLightingPanel(session,manager_&&manager_->Status().Busy());
    if(action&&manager_&&session.Source()){
        const auto previous=manager_->options;
        manager_->options.lighting=action==2?"manual-test":action==4?"intrinsic-assisted":"robust-directional-ambient";
        manager_->options.calibration=session.lighting.source;
        manager_->Start(session.Source()->packageRoot,action!=3&&action!=5,action==3,action==5);statusOpen_=true;
        manager_->options=previous; // An offline manual export must not change defaults for the next image.
    }
}
float ReconstructionPanel::Draw(){
    if(!manager_)return 0;
    try{if(auto selected=picker_.Poll();selected&&!selected->empty()){
        if(pythonPicker_){manager_->options.python=*selected;const auto text=PathUtf8(*selected);strncpy_s(pythonPath_.data(),pythonPath_.size(),text.c_str(),_TRUNCATE);}
        else {manager_->Start(*selected);statusOpen_=true;}
    }}catch(const std::exception& e){manager_->Fail(e.what());statusOpen_=true;}
    const auto status=manager_->Status();if(status.Busy())statusOpen_=true;
    float menuHeight=0;
    if(ImGui::BeginMainMenuBar()){
        menuHeight=ImGui::GetWindowHeight();
        if(ImGui::BeginMenu("文件###File")){
            if(ImGui::MenuItem("从图像重建…###Reconstruct Image...",nullptr,false,!status.Busy()&&!picker_.Busy())){pythonPicker_=false;picker_.Open(owner_);}
            if(ImGui::MenuItem("重建设置…###Reconstruction settings...",nullptr,false,!status.Busy())){
                settingsOpen_=true;const auto text=PathUtf8(manager_->options.python);strncpy_s(pythonPath_.data(),pythonPath_.size(),text.c_str(),_TRUNCATE);}
            if(ImGui::MenuItem("重建状态###Reconstruction status"))statusOpen_=true;
            ImGui::EndMenu();
        }
        ImGui::TextDisabled("重建：%s",ChineseText(ReconstructionStateName(status.state)).c_str());ImGui::EndMainMenuBar();
    }
    return menuHeight;
}
void ReconstructionPanel::DrawWindows(){
    if(!manager_)return;
    const auto status=manager_->Status();
    if(settingsOpen_){
        ImGui::SetNextWindowPos({240,90},ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize({600,330},ImGuiCond_FirstUseEver);
        if(ImGui::Begin("重建设置###Reconstruction settings",&settingsOpen_)){
            ImGui::BeginDisabled(status.Busy());
            ImGui::TextWrapped("选择 image-scene-renderer 环境中的 python.exe。模型在独立进程中运行。");
            ImGui::SetNextItemWidth(-90);
            if(ImGui::InputText("##python",pythonPath_.data(),pythonPath_.size()))manager_->options.python=std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(pythonPath_.data())));
            ImGui::SameLine();if(ImGui::Button("浏览###Browse")&&!picker_.Busy()){pythonPicker_=true;picker_.Open(owner_,true);}
            auto combo=[](const char* label,std::string& value,const char* real,const char* fallback){
                int index=value==real?0:1;const auto realText=ChineseText(real),fallbackText=ChineseText(fallback);
                const char* choices[]{realText.c_str(),fallbackText.c_str()};
                if(ImGui::Combo(label,&index,choices,2))value=index==0?real:fallback;};
            auto& config=manager_->options;
            combo("几何###Geometry",config.geometry,"moge","dummy");combo("区域分割###Segmentation",config.segmentation,"sam2","dummy");
            combo("材质###Materials",config.materials,"marigold","neutral");
            combo("光照###Lighting",config.lighting,"robust-directional-ambient","manual-test");
            combo("本征分解（仅离线）###Intrinsic (offline only)",config.intrinsic,"marigold-lighting","proxy");
            int size=static_cast<int>(config.maxSize);if(ImGui::SliderInt("分析图像最长边###Max image edge",&size,128,1024))config.maxSize=static_cast<unsigned>(size);
            ImGui::Checkbox("仅使用已缓存模型（离线）###Use cached models only (offline)",&config.offline);
            ImGui::TextWrapped("默认使用 MoGe、SAM 2 和 Marigold，分析尺寸为 512 像素。占位与中性材质仅用于测试回退。请先用项目安装脚本准备依赖。");
            ImGui::EndDisabled();
        }ImGui::End();
    }
    if(statusOpen_){
        ImGui::SetNextWindowPos({245,125},ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize({510,345},ImGuiCond_FirstUseEver);
        if(ImGui::Begin("图像重建###Reconstruction",&statusOpen_)){
            ImGui::Text("状态：%s",ChineseText(ReconstructionStateName(status.state)).c_str());
            for(size_t i=0;i<status.stages.size();++i){const auto& stage=status.stages[i];
                const char* display=stage=="complete"?"已完成":stage=="running"?"处理中":stage=="error"?"错误":"等待中";
                ImGui::Text("%-16s %s",ChineseText(status.stageNames[i]).c_str(),display);}
            ImGui::Separator();
            ImGui::BeginChild("details",{0,-65});ImGui::TextWrapped("%s",ChineseText(status.message).c_str());
            if(!status.output.empty())ImGui::TextWrapped("输出：%s",PathUtf8(status.output).c_str());
            if(!status.log.empty())ImGui::TextWrapped("日志：%s",PathUtf8(status.log).c_str());ImGui::EndChild();
            if(status.Busy()){ImGui::BeginDisabled(status.cancelRequested);if(ImGui::Button("取消###Cancel"))manager_->Cancel();ImGui::EndDisabled();}
            else if(status.state==ReconstructionState::Error&&!status.input.empty()){
                if(ImGui::Button("重试###Retry"))manager_->Retry();ImGui::SameLine();
                if(ImGui::Button("设置###Settings")){settingsOpen_=true;const auto text=PathUtf8(manager_->options.python);strncpy_s(pythonPath_.data(),pythonPath_.size(),text.c_str(),_TRUNCATE);}}
            if(!status.log.empty()){
                if(status.Busy()||(status.state==ReconstructionState::Error&&!status.input.empty()))ImGui::SameLine();
                if(ImGui::Button("复制日志路径###Copy log path"))ImGui::SetClipboardText(PathUtf8(status.log).c_str());}
        }ImGui::End();
    }
}
}
