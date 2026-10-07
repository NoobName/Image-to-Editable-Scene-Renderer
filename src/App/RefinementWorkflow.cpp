#include "App/RefinementWorkflow.h"
#include "Reconstruction/PythonProcess.h"
#include "Assets/AssetIO.h"
#include "Core/Log.h"
namespace isr {
void RefinementWorkflow::Tick(Renderer& renderer,RecipeActions& a,const ReconstructionOptions& config,bool otherBusy){
    try{
        if(a.cancelRefinement&&a.refinementBusy)stop_.request_stop();
        if(a.request==RecipeAction::Refine){
            a.request=RecipeAction::None;
            if(a.refinementBusy||a.busy||otherBusy)throw std::runtime_error("Wait for the active loading task");
            if(!renderer.Session().CanDisplayImage()||renderer.Session().Mode()!=WorkMode::ImageRelighting)
                throw std::runtime_error("Refinement requires Image Relighting mode with a source anchor");
            if(!renderer.Session().pointLights.empty())throw std::runtime_error("当前神经优化只支持方向光引导；请保留带点光源的物理结果。");
            if(config.python.empty()||!std::filesystem::is_regular_file(config.python))throw std::runtime_error("Select Python in Reconstruction settings");
            if(!std::isfinite(a.refinementStrength)||a.refinementStrength<0||a.refinementStrength>1||a.refinementSeed<0)throw std::runtime_error("Invalid refinement parameters");
            static uint64_t sequence=0;
            const auto root=config.projectRoot/"generated"/("refinement-job-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(++sequence));
            std::filesystem::create_directory(root);
            // Snapshot/export only on this explicit action. No per-frame GPU readback and
            // no neural result ever replaces the current source/physics GPU descriptors.
            SaveRecipe(root/"physics.json",renderer.Session(),renderer.ImportedProtection(),false);
            renderer.ExportImage(root/"physics");
            revision_=renderer.Session().Revision();state_=RecipeState(renderer.Session()).dump();
            protection_=renderer.ImportedProtection();
            stop_=std::stop_source{};const auto token=stop_.get_token();a.cancelRefinement=false;
            const auto strength=a.refinementStrength;const auto seed=a.refinementSeed;
            worker_=std::async(std::launch::async,[=]{
                const std::vector<std::wstring> args{(config.projectRoot/"tools/reconstruction/refine_image.py").wstring(),
                    L"--recipe",(root/"physics.json").wstring(),L"--physics-export",(root/"physics").wstring(),
                    L"--output",(root/"candidate").wstring(),L"--strength",std::to_wstring(strength),L"--seed",std::to_wstring(seed)};
                const auto exit=RunPythonProcess(config.python,args,config.projectRoot,root/"process.log",token,[]{});
                return Result{exit,root/"candidate/comparison.html"};
            });
            a.refinementBusy=true;a.refinementStatus="Offline refinement running; physics remains interactive.";
            Log("Refinement job: "+PathUtf8(root));
        }
        if(worker_.valid()&&worker_.wait_for(std::chrono::seconds(0))==std::future_status::ready){
            const auto result=worker_.get();a.refinementBusy=false;
            if(a.cancelRefinement||otherBusy||renderer.Session().Revision()!=revision_||RecipeState(renderer.Session()).dump()!=state_||renderer.ImportedProtection()!=protection_){
                a.refinementStatus="Cancelled/stale candidate; current physics and previous comparison retained.";
            }else{
                if(std::filesystem::is_regular_file(result.report))a.refinementReport=result.report;
                a.refinementStatus=result.exit==0?"Offline candidate ready. Inspect content changes in the comparison before using it.":"Refinement failed/unavailable; physics retained. Inspect the process log or fallback comparison.";
            }
            Log(a.refinementStatus);
        }
    }catch(const std::exception& e){a.refinementBusy=false;a.refinementStatus=std::string("Refinement stopped; physics retained: ")+e.what();Log(a.refinementStatus);}
}
}
