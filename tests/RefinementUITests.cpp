#include "UI/RecipePanel.h"
#include <imgui.h>
#include <iostream>
#include <stdexcept>
using namespace isr;
int main(){try{
    ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize={640,480};io.DeltaTime=1.f/60;
    unsigned char* pixels;int w,h;io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
    RecipeActions actions;ImVec2 lo{},hi{};bool disabled=false;
    auto frame=[&]{ImGui::NewFrame();ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize({600,400});ImGui::Begin("Test");
        ImGui::BeginDisabled(disabled);DrawRefinementButton(actions);lo=ImGui::GetItemRectMin();hi=ImGui::GetItemRectMax();ImGui::EndDisabled();ImGui::End();ImGui::Render();};
    frame();frame();if(actions.request!=RecipeAction::None)throw std::runtime_error("Refinement triggered without an explicit event");
    io.AddMousePosEvent((lo.x+hi.x)*.5f,(lo.y+hi.y)*.5f);frame();io.AddMouseButtonEvent(0,true);frame();io.AddMouseButtonEvent(0,false);frame();
    if(actions.request!=RecipeAction::Refine)throw std::runtime_error("Explicit ImGui click did not request refinement");
    actions.request=RecipeAction::None;disabled=true;frame();io.AddMouseButtonEvent(0,true);frame();io.AddMouseButtonEvent(0,false);frame();
    if(actions.request!=RecipeAction::None)throw std::runtime_error("Busy/disabled control launched refinement");
    ImGui::DestroyContext();std::cout<<"Explicit refinement ImGui event + disabled isolation: OK\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
