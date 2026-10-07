#include "UI/ViewportTools.h"
#include <imgui.h>
#include <iostream>
#include <cmath>
#include <stdexcept>
using namespace isr;
namespace {void Require(bool v,const char* text){if(!v)throw std::runtime_error(text);}}
int main(){try{
    ImGui::CreateContext();struct Cleanup{~Cleanup(){ImGui::DestroyContext();}}cleanup;
    auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize={1000,720};io.DeltaTime=1.f/60;io.ConfigInputTrickleEventQueue=false;
    unsigned char* pixels;int width,height;io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
    Scene scene;scene.meshes={Mesh::Cube()};scene.materials.resize(1);scene.lights.push_back(Light{});
    Entity root;root.objectId="object";root.name="Object";root.transform.position={0,0,4};
    Entity child;child.parent=0;child.renderer=MeshRenderer{0,0};scene.entities={root,child};
    scene.camera.LookAt({0,0,0},{0,0,1});scene.camera.SetPerspective(DirectX::XM_PIDIV2,800.f/600,.1f,20);scene.UpdateWorldMatrices();
    SceneEditState edit;edit.Capture(scene);SceneSelection selection{SelectionKind::Camera,0};ViewportTools tools;
    ImVec2 a{},b{},addMin{},addMax{};bool toolMouse=false;
    auto frame=[&]{ImGui::NewFrame();ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize({830,670});
        ImGui::Begin("Viewport tools test",nullptr,ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize);
        ScenePointAddButton(scene,tools.points);addMin=ImGui::GetItemRectMin();addMax=ImGui::GetItemRectMax();
        ImGui::Image(static_cast<ImTextureID>(0),{800,600});a=ImGui::GetItemRectMin();b=ImGui::GetItemRectMax();
        toolMouse=tools.Draw(scene,selection,edit,a,b,ImGui::IsItemHovered());ImGui::End();ImGui::Render();};
    frame();frame();
    const ImVec2 center{(a.x+b.x)*.5f,(a.y+b.y)*.5f};
    io.AddMousePosEvent(center.x,center.y);frame();io.AddMouseButtonEvent(0,true);frame();
    Require(selection.kind==SelectionKind::Entity&&selection.index==0,"Actual ImGui click failed to select logical object");
    io.AddMouseButtonEvent(0,false);frame();
    const ImVec2 sun{b.x-79,a.y+87};io.AddMousePosEvent(sun.x,sun.y);
    const auto initial=scene.lights[0].direction;io.AddMouseButtonEvent(0,true);frame();
    Require(selection.kind==SelectionKind::Light&&toolMouse,"Gizmo press did not capture/select light");
    Require(initial.x==scene.lights[0].direction.x&&initial.y==scene.lights[0].direction.y&&initial.z==scene.lights[0].direction.z,"Gizmo jumped on initial click");
    io.AddMousePosEvent(sun.x+20,sun.y-15);frame();const auto changed=scene.lights[0].direction;
    Require(std::abs(initial.x-changed.x)+std::abs(initial.y-changed.y)+std::abs(initial.z-changed.z)>.01f,"ImGui drag did not update real light data");
    Require(std::abs(changed.x*changed.x+changed.y*changed.y+changed.z*changed.z-1)<1e-4f,"Gizmo produced nonunit vector");
    io.AddMousePosEvent(b.x+80,sun.y);frame();Require(toolMouse,"Gizmo lost owned drag outside its bounds");
    io.AddMouseButtonEvent(0,false);frame();frame();Require(!toolMouse,"Released gizmo retained mouse ownership");
    auto beforeRight=scene.lights[0].direction;
    io.AddMousePosEvent(sun.x,sun.y);io.AddMouseButtonEvent(1,true);frame();io.AddMousePosEvent(sun.x+5,sun.y+5);frame();
    auto afterRight=scene.lights[0].direction;Require(beforeRight.x==afterRight.x&&beforeRight.y==afterRight.y&&beforeRight.z==afterRight.z,"RMB rotated the sun");
    io.AddMouseButtonEvent(1,false);frame();
    io.AddMousePosEvent((addMin.x+addMax.x)*.5f,(addMin.y+addMax.y)*.5f);frame();io.AddMouseButtonEvent(0,true);frame();io.AddMouseButtonEvent(0,false);frame();
    Require(tools.points.placing,"Actual add point button did not enter placement");
    io.AddMousePosEvent((a.x+b.x)*.5f,(a.y+b.y)*.5f);frame();io.AddMouseButtonEvent(0,true);frame();io.AddMouseButtonEvent(0,false);frame();
    Require(scene.lights.size()==2&&scene.lights[1].type==LightType::Point&&selection.index==1,"Scene point placement failed");
    const auto initialPoint=scene.lights[1].position;Require(initialPoint.z>0&&initialPoint.z<4,"Initial point was not placed in front of observed surface");
    io.AddMousePosEvent((a.x+b.x)*.5f,(a.y+b.y)*.5f);frame();frame();io.AddMouseButtonEvent(0,true);frame();io.AddMousePosEvent((a.x+b.x)*.5f+35,(a.y+b.y)*.5f-20);frame();io.AddMouseButtonEvent(0,false);frame();
    Require(scene.lights[1].position.x>initialPoint.x&&scene.lights[1].position.y>initialPoint.y&&std::abs(scene.lights[1].position.z-initialPoint.z)<1e-5,"Scene point drag did not retain depth");
    tools.points.placing=true;io.AddKeyEvent(ImGuiKey_Escape,true);frame();io.AddKeyEvent(ImGuiKey_Escape,false);frame();Require(!tools.points.placing&&scene.lights.size()==2,"Esc did not cancel scene placement");
    scene.lights.clear();tools.Reset();frame();Require(!toolMouse,"No-light scene retained gizmo capture");
    std::cout<<"ImGui viewport picking, sun drag, light selection, outside drag, release and RMB isolation: OK\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
