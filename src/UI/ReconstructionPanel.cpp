#include "UI/ReconstructionPanel.h"
#include "Assets/AssetIO.h"
#include "UI/LightingPanel.h"
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
        if(ImGui::BeginMenu("File")){
            if(ImGui::MenuItem("Reconstruct Image...",nullptr,false,!status.Busy()&&!picker_.Busy())){pythonPicker_=false;picker_.Open(owner_);}
            if(ImGui::MenuItem("Reconstruction settings...",nullptr,false,!status.Busy())){
                settingsOpen_=true;const auto text=PathUtf8(manager_->options.python);strncpy_s(pythonPath_.data(),pythonPath_.size(),text.c_str(),_TRUNCATE);}
            if(ImGui::MenuItem("Reconstruction status"))statusOpen_=true;
            ImGui::EndMenu();
        }
        ImGui::TextDisabled("Reconstruction: %s",ReconstructionStateName(status.state));ImGui::EndMainMenuBar();
    }
    return menuHeight;
}
void ReconstructionPanel::DrawWindows(){
    if(!manager_)return;
    const auto status=manager_->Status();
    if(settingsOpen_){
        ImGui::SetNextWindowPos({240,90},ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize({600,330},ImGuiCond_FirstUseEver);
        if(ImGui::Begin("Reconstruction settings",&settingsOpen_)){
            ImGui::BeginDisabled(status.Busy());
            ImGui::TextWrapped("Select python.exe from the image-scene-renderer environment. Models run in a separate process.");
            ImGui::SetNextItemWidth(-90);
            if(ImGui::InputText("##python",pythonPath_.data(),pythonPath_.size()))manager_->options.python=std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(pythonPath_.data())));
            ImGui::SameLine();if(ImGui::Button("Browse")&&!picker_.Busy()){pythonPicker_=true;picker_.Open(owner_,true);}
            auto combo=[](const char* label,std::string& value,const char* real,const char* fallback){
                int index=value==real?0:1;const char* choices[]{real,fallback};if(ImGui::Combo(label,&index,choices,2))value=choices[index];};
            auto& config=manager_->options;
            combo("Geometry",config.geometry,"moge","dummy");combo("Segmentation",config.segmentation,"sam2","dummy");
            combo("Materials",config.materials,"marigold","neutral");
            combo("Lighting",config.lighting,"robust-directional-ambient","manual-test");
            combo("Intrinsic (offline only)",config.intrinsic,"marigold-lighting","proxy");
            int size=static_cast<int>(config.maxSize);if(ImGui::SliderInt("Max image edge",&size,128,1024))config.maxSize=static_cast<unsigned>(size);
            ImGui::Checkbox("Use cached models only (offline)",&config.offline);
            ImGui::TextWrapped("Default: MoGe + SAM 2 + Marigold, 512 pixels. Dummy/neutral are explicit test fallbacks. Install dependencies with the project's setup scripts first.");
            ImGui::EndDisabled();
        }ImGui::End();
    }
    if(statusOpen_){
        ImGui::SetNextWindowPos({245,125},ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize({510,345},ImGuiCond_FirstUseEver);
        if(ImGui::Begin("Reconstruction",&statusOpen_)){
            ImGui::Text("State: %s",ReconstructionStateName(status.state));
            for(size_t i=0;i<status.stages.size();++i){const auto& stage=status.stages[i];
                const char* display=stage=="complete"?"Complete":stage=="running"?"Processing":stage=="error"?"Error":"Pending";
                ImGui::Text("%-16s %s",status.stageNames[i].c_str(),display);}
            ImGui::Separator();
            ImGui::BeginChild("details",{0,-65});ImGui::TextWrapped("%s",status.message.c_str());
            if(!status.output.empty())ImGui::TextWrapped("Output: %s",PathUtf8(status.output).c_str());
            if(!status.log.empty())ImGui::TextWrapped("Log: %s",PathUtf8(status.log).c_str());ImGui::EndChild();
            if(status.Busy()){ImGui::BeginDisabled(status.cancelRequested);if(ImGui::Button("Cancel"))manager_->Cancel();ImGui::EndDisabled();}
            else if(status.state==ReconstructionState::Error&&!status.input.empty()){
                if(ImGui::Button("Retry"))manager_->Retry();ImGui::SameLine();
                if(ImGui::Button("Settings")){settingsOpen_=true;const auto text=PathUtf8(manager_->options.python);strncpy_s(pythonPath_.data(),pythonPath_.size(),text.c_str(),_TRUNCATE);}}
            if(!status.log.empty()){
                if(status.Busy()||(status.state==ReconstructionState::Error&&!status.input.empty()))ImGui::SameLine();
                if(ImGui::Button("Copy log path"))ImGui::SetClipboardText(PathUtf8(status.log).c_str());}
        }ImGui::End();
    }
}
}
