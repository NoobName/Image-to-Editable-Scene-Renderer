#pragma once
#include "ScenePackage/ScenePackageLoader.h"
#include <array>
#include <mutex>
#include <thread>
namespace isr {
enum class ReconstructionState { Idle, Processing, Loading, Ready, Error };
struct ReconstructionOptions {
    std::filesystem::path projectRoot,python;
    std::string geometry="moge",segmentation="sam2",materials="marigold";
    std::string lighting="robust-directional-ambient";
    std::string intrinsic="marigold-lighting";
    LightingParameters calibration;
    bool lightingOnly=false,intrinsicOnly=false,shadowOnly=false;
    unsigned maxSize=512;
    bool offline=true;
};
struct ReconstructionStatus {
    ReconstructionState state=ReconstructionState::Idle;
    std::vector<std::string> stageNames{"geometry","segmentation","materials","lighting","export"};
    std::vector<std::string> stages{"pending","pending","pending","pending","pending"};
    bool lightingOnly=false,intrinsicOnly=false,shadowOnly=false;
    std::string message="Choose File > Reconstruct Image",jobId;
    std::filesystem::path input,output,log;
    bool cancelRequested=false;
    bool Busy()const{return state==ReconstructionState::Processing||state==ReconstructionState::Loading;}
};
class ReconstructionManager {
public:
    explicit ReconstructionManager(std::filesystem::path projectRoot);
    ~ReconstructionManager();
    ReconstructionOptions options; // UI/main thread only; snapshotted at Start.
    bool Start(const std::filesystem::path& input,bool lightingOnly=false,bool intrinsicOnly=false,bool shadowOnly=false);
    bool Retry();
    void Cancel();
    ReconstructionStatus Status()const;
    std::unique_ptr<ScenePackage> TakePackage(); // Once CPU loading has finished.
    void Ready(); // Only after the renderer has committed the GPU scene.
    void Fail(std::string message);
    static std::filesystem::path FindProjectRoot(const std::filesystem::path& executableDirectory);
private:
    void Run(std::stop_token,ReconstructionOptions,std::filesystem::path input);
    void ReadProgress(const std::filesystem::path&,bool final=false);
    mutable std::mutex mutex_;
    ReconstructionStatus status_;
    std::unique_ptr<ScenePackage> package_;
    uint64_t sequence_=0;
    std::jthread worker_;
    ReconstructionOptions lastOptions_; // Main-thread retry snapshot; worker receives its own value.
};
const char* ReconstructionStateName(ReconstructionState);
std::pair<std::vector<std::string>,std::vector<std::string>> ParseProgressTable(const package::Json&);
}
