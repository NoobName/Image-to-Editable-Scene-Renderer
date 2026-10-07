#pragma once
#include "UI/AsyncFileDialog.h"
#include "Reconstruction/ReconstructionManager.h"
#include "ScenePackage/RelightingSession.h"
#include <array>
namespace isr {
class ReconstructionPanel {
public:
    void Bind(HWND owner,ReconstructionManager* manager){owner_=owner;manager_=manager;}
    float Draw(); // Menu height; also displays progress/settings without blocking render.
    void DrawWindows();
    bool Busy()const{return manager_&&manager_->Status().Busy();}
    void DrawLighting(RelightingSession&);
private:
    HWND owner_=nullptr;
    ReconstructionManager* manager_=nullptr;
    AsyncFileDialog picker_;
    bool pythonPicker_=false,settingsOpen_=false,statusOpen_=false;
    std::array<char,4096> pythonPath_{};
};
}
