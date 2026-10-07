#pragma once
#include "UI/ReferencePanel.h"
#include "Reconstruction/ReconstructionManager.h"
#include "Renderer/Renderer.h"
#include <stop_token>
namespace isr {
class ReferenceWorkflow {
public:
    ~ReferenceWorkflow(){stop_.request_stop();if(worker_.valid())worker_.wait();}
    ReferenceActions actions;
    bool autoApply=false,resetAfterApply=false;
    void Tick(Renderer&,const ReconstructionOptions&,bool otherBusy);
private:
    std::future<std::shared_ptr<const ReferenceAnalysis>> worker_;
    std::stop_source stop_;
    uint64_t sceneRevision_{};
};
}
