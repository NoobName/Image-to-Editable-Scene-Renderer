#include "UI/LightingPanel.h"
#include "Reconstruction/ReconstructionManager.h"
#include <imgui.h>
#include <iostream>
using namespace isr;
namespace {void Require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}}
int main(){try{
    auto data=std::make_shared<LightingData>();data->source.direction={.6f,0,.8f};data->target=data->source;
    LightingSession state;state.Publish(data.get());const auto fixed=state.source;
    state.target.directIntensity=7;state.target.direction={1,0,0};
    Require(state.source==fixed&&state.cacheValid,"Target edited source or cache");
    state.draft.direction={0,0,0};Require(!state.ApplySource()&&state.source==fixed&&state.cacheValid,"Zero direction applied");
    state.draft=state.source;state.draft.directIntensity=2;
    ImGui::CreateContext();struct Cleanup{~Cleanup(){ImGui::DestroyContext();}}cleanup;
    auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize={500,250};io.DeltaTime=1.f/60;io.ConfigInputTrickleEventQueue=false;
    unsigned char* pixels;int w,h;io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);ImVec2 low{},high{};
    auto frame=[&]{ImGui::NewFrame();ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize({450,200});ImGui::Begin("Calibration");
        DrawApplySourceButton(state);low=ImGui::GetItemRectMin();high=ImGui::GetItemRectMax();ImGui::End();ImGui::Render();};
    frame();frame();Require(state.source==fixed,"Draft auto-applied before button");
    io.AddMousePosEvent((low.x+high.x)*.5f,(low.y+high.y)*.5f);frame();
    io.AddMouseButtonEvent(0,true);frame();io.AddMouseButtonEvent(0,false);frame();
    Require(state.source.directIntensity==2&&!state.cacheValid&&state.manualSource&&state.sourceRevision==1,"Actual Apply ImGui event failed to invalidate");
    Require(state.target.directIntensity==7,"Source Apply silently changed target");
    state.Publish(data.get());Require(state.cacheValid&&state.target==data->target&&state.sourceRevision==0,"Commit did not replace whole lighting session");
    state.Publish(nullptr);Require(!state.available&&!state.cacheValid,"Legacy package retained stale lighting");
    package::Json old={{"version",1},{"stages",{{"geometry","complete"},{"segmentation","complete"},{"materials","complete"},{"export","complete"}}}};
    Require(ParseProgressTable(old).first.size()==4,"Legacy progress incompatible");
    package::Json offline={{"version",2},{"stage_order",{"lighting","export"}},{"stages",{{"lighting","complete"},{"export","pending"}}}};
    Require(ParseProgressTable(offline).first.size()==2,"Offline stage table incompatible");
    offline["stage_order"]={"export","lighting"};bool rejected=false;try{ParseProgressTable(offline);}catch(...){rejected=true;}
    Require(rejected,"Unrecognized stage table accepted");
    std::cout<<"Lighting isolation, explicit ImGui Apply, cache invalidation and progress v1/v2: OK\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
