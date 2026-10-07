#pragma once
#include "App/RecipeWorkflow.h"
#include "Reconstruction/ReconstructionManager.h"
#include <stop_token>
namespace isr {
class RefinementWorkflow {
public:
    ~RefinementWorkflow(){stop_.request_stop();if(worker_.valid())worker_.wait();}
    void Tick(Renderer&,RecipeActions&,const ReconstructionOptions&,bool otherBusy);
private:
    struct Result {unsigned exit{};std::filesystem::path report;};
    std::future<Result> worker_;
    std::stop_source stop_;
    uint64_t revision_{};
    std::string state_;
    std::shared_ptr<const ProtectionMask> protection_;
};
}
