#include "UI/ReferencePanel.h"
#include "ScenePackage/RelightingRecipe.h"
#include <imgui.h>
#include <iostream>
using namespace isr;
namespace {void Need(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}}
int wmain(int argc,wchar_t** argv){try{
    if(argc>1){auto data=LoadReferenceAnalysis(argv[1]);if(argc>2){auto loaded=ScenePackageLoader{}.Load(argv[2]);RelightingSession s;s.Publish(loaded.observation);CheckReferenceSource(*data,s);}
        std::cout<<"Reference strict validation passed; canApply="<<data->canApply<<" confidence="<<data->confidence<<'\n';return 0;}
    auto source=std::make_shared<SourceObservation>();source->anchor.emplace();source->anchor->pixels=std::make_shared<ImageData>();
    source->anchor->metadata={{"sourceId","fixture"},{"sourceImage",{{"sha256",std::string(64,'a')}}}};source->regions.push_back({7,"stable","region"});
    RelightingSession s;s.Publish(source);s.lighting.target.directIntensity=2;s.lighting.targetGlobalGain=1.3f;s.display.exposure=.4f;s.protection.Apply(7,.6f);
    auto data=std::make_shared<ReferenceAnalysis>();data->sourceBaseline=s.lighting.source;data->target=s.lighting.target;data->target.direction={1,0,0};data->canApply=true;
    data->metadata={{"source",{{"sourceId","fixture"},{"anchorSha256",std::string(64,'a')},{"baselineRevision",0}}}};s.reference.analysis=data;
    const auto before=RecipeState(s);const auto fixed=s.lighting.source;
    ImGui::CreateContext();struct Cleanup{~Cleanup(){ImGui::DestroyContext();}}cleanup;auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize={500,250};io.DeltaTime=1.f/60;io.ConfigInputTrickleEventQueue=false;
    unsigned char* pixels;int w,h;io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);ImVec2 low{},high{};bool reset=false,optimize=false;ReferenceActions actions;
    auto frame=[&]{ImGui::NewFrame();ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize({450,200});ImGui::Begin("Reference");
        if(optimize)DrawOptimizeTargetButton(s,actions);else if(reset)DrawResetReferenceButton(s);else DrawApplyReferenceButton(s);low=ImGui::GetItemRectMin();high=ImGui::GetItemRectMax();ImGui::End();ImGui::Render();};
    auto click=[&]{frame();frame();io.AddMousePosEvent((low.x+high.x)*.5f,(low.y+high.y)*.5f);frame();io.AddMouseButtonEvent(0,true);frame();io.AddMouseButtonEvent(0,false);frame();};
    click();Need(s.reference.applied&&s.lighting.target==data->target&&s.lighting.source==fixed,"Actual Apply event or source isolation");
    auto expected=before;expected["target"]=LightingJson(data->target);expected["targetGlobalGain"]=1;Need(RecipeState(s)==expected,"Apply altered display/protection/source");
    RelightingSession reopened;reopened.Publish(source);ApplyRecipeState(reopened,RecipeState(s));Need(RecipeState(reopened)==expected,"Recipe loses reference target");
    reset=true;click();Need(!s.reference.applied&&RecipeState(s)==before,"Actual Restore event failed");
    data->canApply=false;Need(!ApplyReferenceProposal(s)&&RecipeState(s)==before,"Unavailable proposal applied");data->canApply=true;
    data->metadata["source"]["sourceId"]="wrong";bool rejected=false;try{ApplyReferenceProposal(s);}catch(...){rejected=true;}Need(rejected&&RecipeState(s)==before,"Wrong source changed state");
    data->metadata["source"]["sourceId"]="fixture";data->metadata["source"]["baselineRevision"]=1;rejected=false;try{ApplyReferenceProposal(s);}catch(...){rejected=true;}Need(rejected&&RecipeState(s)==before,"Stale calibration changed state");
    data->metadata["source"]["baselineRevision"]=0;optimize=true;click();Need(actions.request==ReferenceAction::Optimize&&RecipeState(s)==before,"Actual Optimize button changed target or did not dispatch");
    data->target=s.lighting.source;data->target.direction={1,0,0};
    data->optimization={{"initialState",before},{"initialEffectiveTarget",LightingJson(s.lighting.EffectiveTarget())}};
    rejected=false;try{ApplyReferenceProposal(s);}catch(...){rejected=true;}Need(rejected&&RecipeState(s)==before,"Unverified optimization applied");
    s.reference.gpuVerified=true;optimize=false;reset=false;click();Need(s.reference.applied&&s.lighting.source==fixed,"Verified optimization Apply failed");reset=true;click();Need(RecipeState(s)==before,"Optimization Restore failed");
    s.lighting.target.directIntensity=.123f;const auto edited=RecipeState(s);rejected=false;try{ApplyReferenceProposal(s);}catch(...){rejected=true;}Need(rejected&&RecipeState(s)==edited,"Concurrent target change was overwritten");
    std::cout<<"Actual ImGui Apply/Restore/Optimize, GPU gate, recipe identity, source isolation and failure retention passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
