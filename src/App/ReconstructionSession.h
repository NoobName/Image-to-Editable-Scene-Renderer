#pragma once
#include "Reconstruction/ReconstructionManager.h"
#include "Renderer/Renderer.h"
#include "App/CameraController.h"
namespace isr {
class ReconstructionSession {
public:
    explicit ReconstructionSession(const std::filesystem::path& executableDirectory)
        :manager(ReconstructionManager::FindProjectRoot(executableDirectory)){}
    ReconstructionManager manager;
    void Tick(Renderer&,Scene&,RenderSettings&,CameraController&);
private:
    std::shared_ptr<ScenePackage> pending_;
};
}
