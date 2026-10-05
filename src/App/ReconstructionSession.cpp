#include "App/ReconstructionSession.h"
namespace isr {
void ReconstructionSession::Tick(Renderer& renderer,Scene& scene,RenderSettings& settings,CameraController& controller){
    try {
        if(!pending_){
            auto package=manager.TakePackage();
            if(package){
                if(manager.Status().cancelRequested){manager.Fail("Reconstruction cancelled; current scene retained.");return;}
                pending_=std::move(package);renderer.PrepareScene(pending_);
            }
        }
        if(pending_&&renderer.ScenePrepared()){
            if(manager.Status().cancelRequested){renderer.CommitPreparedScene(true);pending_.reset();manager.Fail("Reconstruction cancelled; current scene retained.");return;}
            // Prepare all potentially allocating values before GPU/session publication.
            // The subsequent CPU swaps cannot fail and complete this main-thread transaction.
            auto nextSettings=settings;nextSettings.look=pending_->look;
            const auto& env=pending_->environment;
            nextSettings.environmentPath=env.hdri.empty()?EnvironmentManager::DefaultPath():env.hdri;
            nextSettings.environmentIntensity=env.intensity;nextSettings.environmentRotation=env.rotation;
            nextSettings.ibl=env.ibl;nextSettings.skybox=env.skybox;
            static_assert(std::is_nothrow_move_assignable_v<Scene>&&std::is_nothrow_move_assignable_v<RenderSettings>);
            renderer.CommitPreparedScene();scene=std::move(pending_->scene);settings=std::move(nextSettings);controller=CameraController{};
            pending_.reset();manager.Ready();
        }
    }catch(const std::exception& e){pending_.reset();manager.Fail(std::string("Scene loading failed; current scene retained.\n")+e.what());}
}
}
