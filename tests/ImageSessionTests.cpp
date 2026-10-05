#include "UI/SourceImagePanel.h"
#include "App/CameraController.h"
#include <iostream>
#include <cmath>
using namespace isr;
namespace {void Require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}}
int main(){try{
    RelightingSession session;Require(!session.SetMode(WorkMode::ImageRelighting),"Legacy package entered image mode without anchor");
    auto source=std::make_shared<SourceObservation>();source->packageRoot="test-package";source->anchor.emplace();
    auto pixels=std::make_shared<ImageData>();pixels->width=1500;pixels->height=1000;
    source->anchor->pixels=pixels;source->anchor->sourceSize={1500,1000};source->anchor->analysisSize={512,341};
    source->anchor->metadata={{"sourceCamera",{{"status","synthetic"},{"intrinsics",{1,0,.5,0,1,.5,0,0,1}}}}};
    session.Publish(source);const auto before=session.Source()->anchor->metadata;
    Require(session.SetMode(WorkMode::ImageRelighting),"Valid source cannot enter image mode");session.imageView=ImageDebugView::PixelGrid;
    Camera camera;CameraController controller;const auto position=camera.Position();const auto angles=camera.Angles();const auto aspect=camera.Aspect();
    InputState input;input.keys['R']=input.keys['W']=true;input.rightMouse=true;input.mouseX=400;input.mouseY=80;input.wheel=10;
    for(unsigned i=0;i<20;++i)controller.Update(camera,input,.1f,session.Mode());
    Require(camera.Position().x==position.x&&camera.Position().y==position.y&&camera.Position().z==position.z&&
        camera.Angles().x==angles.x&&camera.Angles().y==angles.y&&camera.Aspect()==aspect,"Image input edited free camera");
    const auto wide=FitSourceImage(1500,1000,1800,600),tall=FitSourceImage(1500,1000,600,1000),native=FitSourceImage(1500,1000,1500,1000);
    Require(wide.x==450&&wide.y==0&&wide.width==900&&wide.height==600,"Wide letterbox wrong");
    Require(tall.x==0&&tall.y==300&&tall.width==600&&tall.height==400,"Narrow letterbox wrong");
    Require(native.scale==1&&native.x==0&&native.y==0,"Native mapping changed centers");
    for(unsigned i=0;i<20;++i){session.SetMode(WorkMode::Scene3D);session.SetMode(WorkMode::ImageRelighting);}
    Require(session.imageView==ImageDebugView::PixelGrid&&session.Source()->anchor->metadata==before,"Mode switch altered independent state");
    ImGui::CreateContext();struct Cleanup{~Cleanup(){ImGui::DestroyContext();}}cleanup;
    auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize={700,300};io.DeltaTime=1.f/60;io.ConfigInputTrickleEventQueue=false;
    unsigned char* font;int w,h;io.Fonts->GetTexDataAsRGBA32(&font,&w,&h);
    ImVec2 imageMin{},imageMax{};
    auto frame=[&]{ImGui::NewFrame();ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize({650,250});ImGui::Begin("Modes");
        DrawWorkModeControls(session);imageMin=ImGui::GetItemRectMin();imageMax=ImGui::GetItemRectMax();ImGui::End();ImGui::Render();};
    frame();frame();const float y=(imageMin.y+imageMax.y)*.5f;
    io.AddMousePosEvent(imageMin.x-45,y);frame();io.AddMouseButtonEvent(0,true);frame();io.AddMouseButtonEvent(0,false);frame();
    Require(session.Mode()==WorkMode::Scene3D,"Actual ImGui 3D mode click failed");
    io.AddMousePosEvent((imageMin.x+imageMax.x)*.5f,y);frame();io.AddMouseButtonEvent(0,true);frame();io.AddMouseButtonEvent(0,false);frame();
    Require(session.Mode()==WorkMode::ImageRelighting,"Actual ImGui image mode click failed");
    session.Publish({});frame();io.AddMouseButtonEvent(0,true);frame();io.AddMouseButtonEvent(0,false);frame();
    Require(session.Mode()==WorkMode::Scene3D,"Disabled image mode accepted click on missing anchor");
    std::cout<<"Image fit, source isolation, camera input, mode state and real ImGui mode events: OK\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
