#include "App/RecipeWorkflow.h"
#include "Core/Log.h"
#include <utility>
namespace isr {
void RecipeWorkflow::Tick(Renderer& renderer,Scene& scene,RenderSettings& settings,CameraController& controller,bool reconstructionBusy){
    try{
        if(actions.request!=RecipeAction::None){const auto request=std::exchange(actions.request,RecipeAction::None);
            if(actions.busy||reconstructionBusy)throw std::runtime_error("Wait for the active loading/reconstruction operation");
            if(request==RecipeAction::Open){actions.busy=true;actions.cancel=false;sceneRevision_=renderer.Session().Revision();const auto path=actions.path;
                actions.status="Validating recipe and package on CPU...";loading_=std::async(std::launch::async,[path]{return LoadRecipe(path);});}
            else if(request==RecipeAction::Export){renderer.ExportImage(actions.path);actions.status="Native PNG/DDS export published.";}
            else {SaveRecipe(actions.path,renderer.Session(),renderer.ImportedProtection(),request==RecipeAction::Replace);actions.status="Recipe published atomically. Source package unchanged.";}
        }
        if(loading_.valid()&&loading_.wait_for(std::chrono::seconds(0))==std::future_status::ready){auto next=loading_.get();
            if(actions.cancel||renderer.Session().Revision()!=sceneRevision_||reconstructionBusy){actions.busy=false;actions.status="Cancelled/stale load; previous document retained.";return;}
            pending_=std::move(next);renderer.PrepareScene(pending_->package,pending_->importedMask,true);actions.status="Uploading recipe scene...";
        }
        if(pending_&&renderer.ScenePrepared()){
            if(actions.cancel||renderer.Session().Revision()!=sceneRevision_||reconstructionBusy){renderer.CommitPreparedScene(true);pending_.reset();actions.busy=false;actions.status="Cancelled/stale upload; previous document retained.";return;}
            auto nextSettings=settings;nextSettings.look=pending_->package->look;const auto& env=pending_->package->environment;
            nextSettings.environmentPath=env.hdri.empty()?EnvironmentManager::DefaultPath():env.hdri;nextSettings.environmentIntensity=env.intensity;nextSettings.environmentRotation=env.rotation;nextSettings.ibl=env.ibl;nextSettings.skybox=env.skybox;
            // Every allocating/validating operation precedes the shared GPU/session/CPU publication.
            renderer.CommitPreparedScene(false,&pending_->session);scene=std::move(pending_->package->scene);settings=std::move(nextSettings);controller=CameraController{};
            pending_.reset();actions.busy=false;actions.status="Recipe restored: explicit source baseline, target, response and protection.";
        }
    }catch(const std::exception& e){pending_.reset();actions.busy=false;actions.status=std::string("Recipe operation failed; previous document/files retained: ")+e.what();Log(actions.status);}
}
}
