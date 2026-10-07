#pragma once
#include "UI/AsyncFileDialog.h"
#include "ScenePackage/RelightingSession.h"
#include <array>
namespace isr {
enum class ReferenceAction {None,Analyze,Load,Optimize};
struct ReferenceActions {
    ReferenceAction request=ReferenceAction::None;
    std::filesystem::path input,log;
    std::string relation="different-content",status="Analyze a reference or open a saved reference.json. No target is changed automatically.";
    bool busy=false,cancel=false,useConfiguredBackends=true,allowFallback=true;
    bool registered=false;
    int iterations=100;
};
bool DrawApplyReferenceButton(RelightingSession&);
bool DrawResetReferenceButton(RelightingSession&);
bool DrawOptimizeTargetButton(const RelightingSession&,ReferenceActions&);
class ReferencePanel {
public:
    void Bind(HWND owner,ReferenceActions* actions){owner_=owner;actions_=actions;}
    void Draw(RelightingSession&,bool otherBusy);
private:
    HWND owner_{};ReferenceActions* actions_{};AsyncFileDialog picker_;
    bool loadProposal_=false;
    std::array<char,4096> path_{};
};
}
