#include "App/ReferenceWorkflow.h"
#include "ScenePackage/RelightingRecipe.h"
#include "Reconstruction/PythonProcess.h"
#include "Core/AtomicFile.h"
#include "Core/Log.h"
#include "Assets/AssetIO.h"
#include "Assets/ImageExport.h"
#include <utility>
namespace isr {
void ReferenceWorkflow::Tick(Renderer& renderer,const ReconstructionOptions& config,bool otherBusy){
    try{
        if(actions.cancel&&actions.busy)stop_.request_stop();
        if(actions.request!=ReferenceAction::None){const auto request=std::exchange(actions.request,ReferenceAction::None);
            if(actions.busy||otherBusy)throw std::runtime_error("Wait for the active loading operation");
            if(!renderer.Session().CanDisplayImage())throw std::runtime_error("Load a source package with an anchor first");
            if(request==ReferenceAction::Optimize&&!renderer.Session().pointLights.empty())throw std::runtime_error("请先清空图像点光源；当前自动优化只评估方向光与环境光。");
            actions.cancel=false;stop_=std::stop_source{};const auto token=stop_.get_token();sceneRevision_=renderer.Session().Revision();
            const auto input=actions.input;const auto source=renderer.Session().Source();const auto baseline=renderer.Session().lighting.source;
            const auto revision=renderer.Session().lighting.sourceRevision;const auto relation=actions.relation;const bool useConfigured=actions.useConfiguredBackends,fallback=actions.allowFallback;
            const auto state=RecipeState(renderer.Session());const auto imported=renderer.ImportedProtection();const bool registered=actions.registered;const int iterations=actions.iterations;
            static uint64_t sequence=0;const auto directory=config.projectRoot/"generated"/("reference-job-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(++sequence));
            std::filesystem::create_directory(directory);actions.log=directory/"process.log";const auto log=actions.log;
            Log("Reference job directory: "+PathUtf8(directory));
            worker_=std::async(std::launch::async,[=]{
                if(request==ReferenceAction::Load)return LoadReferenceAnalysis(input);
                if(config.python.empty()||!std::filesystem::is_regular_file(config.python))throw std::runtime_error("Select Python in File > Reconstruction settings");
                package::Json j={{"sourceId",source->anchor->metadata.at("sourceId")},{"anchorSha256",source->anchor->metadata.at("sourceImage").at("sha256")},
                    {"baseline",LightingJson(baseline)},{"baselineRevision",revision}};
                if(request==ReferenceAction::Optimize){
                    package::Json snapshot={{"source",j},{"initialState",state}};
                    if(imported){const auto mask=directory/"imported-mask.dds";WriteNumericDds(mask,imported->image);snapshot["importedMask"]=PathUtf8(mask);}
                    j=std::move(snapshot);
                }
                const auto text=j.dump(2);const auto path=directory/"request.json";AtomicWrite(path,std::span(reinterpret_cast<const uint8_t*>(text.data()),text.size()));
                std::vector<std::wstring> args{(config.projectRoot/"tools/reconstruction/match_reference.py").wstring(),std::filesystem::absolute(input).wstring(),
                    L"--source-package",source->packageRoot.wstring(),L"--output",(directory/"result").wstring(),L"--request",path.wstring(),L"--relation",std::filesystem::path(relation).wstring(),
                    L"--geometry-backend",std::filesystem::path(useConfigured?config.geometry:"dummy").wstring(),L"--material-backend",std::filesystem::path(useConfigured?config.materials:"neutral").wstring(),L"--max-size",std::to_wstring(config.maxSize)};
                if(fallback)args.push_back(L"--allow-fallback");
                if(request==ReferenceAction::Optimize){
                    args={(config.projectRoot/"tools/reconstruction/optimize_lighting.py").wstring(),L"--source-package",source->packageRoot.wstring(),
                        L"--reference-analysis",std::filesystem::absolute(input).wstring(),L"--output",(directory/"result").wstring(),L"--request",path.wstring(),L"--iterations",std::to_wstring(iterations)};
                    if(registered)args.push_back(L"--registered");
                }
                const auto code=RunPythonProcess(config.python,args,config.projectRoot,log,token,[]{});
                if(code)throw std::runtime_error("Reference Python failed, exit="+std::to_string(code)+"; inspect "+PathUtf8(log));
                return LoadReferenceAnalysis(directory/"result");
            });
            actions.busy=true;actions.status="Analyzing fixed reference observations in an offline Python process...";Log(actions.status);
        }
        if(worker_.valid()&&worker_.wait_for(std::chrono::seconds(0))==std::future_status::ready){
            auto result=worker_.get();actions.busy=false;
            if(actions.cancel||renderer.Session().Revision()!=sceneRevision_||otherBusy){actions.status="Cancelled/stale reference; previous target and observation retained.";Log(actions.status);return;}
            renderer.PublishReference(std::move(result));
            actions.status="Reference proposal ready. Inspect confidence, residual and parameter changes before Apply.";
            if(autoApply){const bool applied=ApplyReferenceProposal(renderer.Session());actions.status=applied?"Reference proposal applied; source and Exposure unchanged.":"Reference illumination unavailable; target retained.";
                if(applied&&resetAfterApply){ResetReferenceTarget(renderer.Session());actions.status="Reference target restored to pre-apply snapshot.";}}
            Log(actions.status);
        }
    }catch(const std::exception& e){actions.busy=false;actions.status=std::string("Reference failed; current source/target retained: ")+e.what();Log(actions.status);}
}
}
