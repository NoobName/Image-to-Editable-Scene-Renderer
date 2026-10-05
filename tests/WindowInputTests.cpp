#include "App/Window.h"
#include "App/CameraController.h"
#include "UI/ImGuiInput.h"
#include <imgui.h>
#include <backends/imgui_impl_win32.h>
#include <iostream>
#include <stdexcept>
using namespace isr;
namespace {
void Require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct ImGuiScope {
    Window& window;
    explicit ImGuiScope(Window& target):window(target){
        ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;
        Require(ImGui_ImplWin32_Init(window.Handle()),"ImGui Win32 initialization");
        auto& io=ImGui::GetIO();io.DisplaySize={1280,720};io.DeltaTime=1.0f/60;
        io.ConfigInputTrickleEventQueue=false;
        unsigned char* pixels;int width,height;io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
    }
    ~ImGuiScope(){window.SetMessageHandler({});ImGui_ImplWin32_Shutdown();ImGui::DestroyContext();}
};
void GuiFrame(InputState* input=nullptr){
    // Feed real backend events, but avoid polling the user's desktop cursor in
    // this deterministic window-message integration test.
    ImGui::NewFrame();ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize({390,640});
    ImGui::Begin("Input test inspector");ImGui::Button("A UI control");ImGui::End();
    if(input)FilterImGuiInput(*input);ImGui::Render();
}
}
int main(){try{
    Window window(GetModuleHandleW(nullptr),1280,720);window.Pump();
    ImGuiScope ui(window);unsigned captureChanges=0;
    window.SetMessageHandler([&](HWND hwnd,UINT message,WPARAM wp,LPARAM lp){
        if(message==WM_CAPTURECHANGED){++captureChanges;std::cout<<"Capture changed; new owner is renderer="<<(reinterpret_cast<HWND>(lp)==hwnd)<<'\n';}
        return HandleImGuiMessage(hwnd,message,wp,lp);
    });
    auto send=[&](UINT message,WPARAM buttons,int x,int y){SendMessageW(window.Handle(),message,buttons,MAKELPARAM(x,y));};
    send(WM_MOUSEMOVE,0,800,300);GuiFrame();GuiFrame();
    Require(!ImGui::GetIO().WantCaptureMouse,"Viewport incorrectly belongs to ImGui");
    // Exercise the actual backend + Window WndProc; do not inject InputState directly.
    SendMessageW(window.Handle(),WM_RBUTTONDOWN,MK_RBUTTON,MAKELPARAM(800,300));
    const auto pressed=window.ConsumeInput();
    std::cout<<"After RMB down: held="<<pressed.rightMouse<<", captureChanges="<<captureChanges<<'\n';
    Require(pressed.rightMouse,"Viewport RMB was cancelled during capture acquisition");
    SendMessageW(window.Handle(),WM_MOUSEMOVE,MK_RBUTTON,MAKELPARAM(850,320));
    auto moved=window.ConsumeInput();
    GuiFrame(&moved);
    Require(moved.rightMouse&&moved.mouseX==50&&moved.mouseY==20,"Viewport drag did not reach camera input");
    Camera camera;CameraController controller;const auto before=camera.View();controller.Update(camera,moved,1.0f/60);
    Require(DirectX::XMVectorGetX(before.r[0])!=DirectX::XMVectorGetX(camera.View().r[0]),"Window drag did not rotate camera");
    SendMessageW(window.Handle(),WM_CAPTURECHANGED,0,reinterpret_cast<LPARAM>(window.Handle()));
    Require(window.ConsumeInput().rightMouse,"Same-owner notification cancelled a drag");
    send(WM_MOUSEMOVE,MK_RBUTTON,880,340);
    send(WM_RBUTTONUP,0,880,340);
    auto released=window.ConsumeInput();GuiFrame(&released);
    Require(!released.rightMouse&&released.mouseX==30&&released.mouseY==20,"Button-up discarded the last drag delta");
    const auto finalMove=camera.View();controller.Update(camera,released,1.0f/60);
    Require(DirectX::XMVectorGetX(finalMove.r[0])!=DirectX::XMVectorGetX(camera.View().r[0]),"Quick drag release did not rotate camera");
    auto consumed=window.ConsumeInput();Require(consumed.mouseX==0&&consumed.mouseY==0,"Drag delta applied more than once");
    send(WM_MOUSEMOVE,0,900,360);Require(window.ConsumeInput().mouseX==0,"Unpressed mouse generated camera motion");
    send(WM_MOUSEMOVE,0,100,200);GuiFrame();GuiFrame();
    Require(ImGui::GetIO().WantCaptureMouse,"Inspector not recognized as UI");
    send(WM_RBUTTONDOWN,MK_RBUTTON,100,200);send(WM_MOUSEMOVE,MK_RBUTTON,120,220);
    auto panel=window.ConsumeInput();GuiFrame(&panel);
    Require(!panel.rightMouse&&panel.mouseX==0&&panel.mouseY==0,"UI drag leaked to camera");
    send(WM_RBUTTONUP,0,120,220);GuiFrame();
    send(WM_MOUSEMOVE,0,800,300);GuiFrame();GuiFrame();
    send(WM_RBUTTONDOWN,MK_RBUTTON,800,300);send(WM_MOUSEMOVE,MK_RBUTTON,820,310);
    SendMessageW(window.Handle(),WM_CAPTURECHANGED,0,0); // Real loss, unlike a same-HWND notification.
    auto lost=window.ConsumeInput();Require(!lost.rightMouse&&lost.mouseX==0,"Capture loss left a stuck drag");
    send(WM_RBUTTONUP,0,820,310);GuiFrame();
    send(WM_RBUTTONDOWN,MK_RBUTTON,800,300);send(WM_MOUSEMOVE,MK_RBUTTON,820,310);
    SendMessageW(window.Handle(),WM_ACTIVATEAPP,FALSE,0);
    auto inactive=window.ConsumeInput();Require(!inactive.active&&!inactive.rightMouse&&inactive.mouseX==0,"Focus loss left camera input active");
    send(WM_RBUTTONUP,0,820,310);GuiFrame();
    // The same Window must work without ImGui's backend acquiring capture first.
    window.SetMessageHandler({});
    SendMessageW(window.Handle(),WM_ACTIVATEAPP,TRUE,0);
    send(WM_RBUTTONDOWN,MK_RBUTTON,800,300);send(WM_MOUSEMOVE,MK_RBUTTON,840,320);send(WM_RBUTTONUP,0,840,320);
    auto plain=window.ConsumeInput();Require(plain.active&&!plain.rightMouse&&plain.mouseX==40&&plain.mouseY==20,"No-UI quick drag failed");
    ViewportInput viewportInput;
    window.SetMessageHandler([&](HWND hwnd,UINT message,WPARAM wp,LPARAM lp){return viewportInput.HandleMessage(hwnd,message,wp,lp);});
    auto viewportFrame=[&]{
        ImGui::NewFrame();ImGui::SetNextWindowPos({400,0});ImGui::SetNextWindowSize({500,640});
        ImGui::Begin("Embedded viewport");ImGui::Image(static_cast<ImTextureID>(0),{480,580});
        const auto min=ImGui::GetItemRectMin(),max=ImGui::GetItemRectMax();
        viewportInput.SetRegion({LONG(min.x),LONG(min.y),LONG(max.x),LONG(max.y)},ImGui::IsItemHovered(),false);
        ImGui::End();ImGui::Render();
    };
    send(WM_MOUSEMOVE,0,600,300);viewportFrame();viewportFrame();
    Require(ImGui::GetIO().WantCaptureMouse,"Embedded viewport should belong to ImGui");
    send(WM_RBUTTONDOWN,MK_RBUTTON,600,300);send(WM_MOUSEMOVE,MK_RBUTTON,650,320);
    auto embedded=window.ConsumeInput();viewportInput.Filter(embedded);
    Require(embedded.rightMouse&&embedded.mouseX==50&&embedded.mouseY==20,"Embedded ImGui image blocked camera drag");
    send(WM_MOUSEMOVE,MK_RBUTTON,100,200);send(WM_RBUTTONUP,0,100,200);
    auto outsideRelease=window.ConsumeInput();viewportInput.Filter(outsideRelease);
    Require(!outsideRelease.rightMouse&&outsideRelease.mouseX==-550,"Release outside viewport discarded owned motion");
    send(WM_RBUTTONDOWN,MK_RBUTTON,100,200);send(WM_MOUSEMOVE,MK_RBUTTON,600,300);
    auto crossed=window.ConsumeInput();viewportInput.Filter(crossed);
    Require(!crossed.rightMouse&&crossed.mouseX==0,"Panel-started drag entered viewport camera");
    send(WM_RBUTTONUP,0,600,300);viewportFrame();viewportFrame();
    send(WM_RBUTTONDOWN,MK_RBUTTON,600,300);send(WM_MOUSEMOVE,MK_RBUTTON,630,320);send(WM_RBUTTONUP,0,630,320);
    auto quick=window.ConsumeInput();viewportInput.Filter(quick);
    Require(!quick.rightMouse&&quick.mouseX==30,"Embedded viewport quick drag lost motion");
    viewportInput.SetRegion({400,20,900,640},true,true); // Popup or active text edit.
    Require(!viewportInput.HandleMessage(window.Handle(),WM_SYSKEYDOWN,VK_F4,LPARAM(1)<<29),"UI blocked native Alt+F4");
    send(WM_RBUTTONDOWN,MK_RBUTTON,600,300);SendMessageW(window.Handle(),WM_KEYDOWN,'W',0);
    auto blocked=window.ConsumeInput();viewportInput.Filter(blocked);
    Require(!blocked.rightMouse&&!blocked.keys['W'],"Popup/text edit leaked camera input");
    send(WM_RBUTTONUP,0,600,300);SendMessageW(window.Handle(),WM_KEYUP,'W',0);
    viewportInput.SetRegion({400,20,900,640},true,false);
    POINT wheel{600,300};ClientToScreen(window.Handle(),&wheel);
    SendMessageW(window.Handle(),WM_MOUSEWHEEL,MAKEWPARAM(0,WHEEL_DELTA),MAKELPARAM(wheel.x,wheel.y));
    Require(window.ConsumeInput().wheel==1,"Viewport wheel was swallowed");
    wheel={100,200};ClientToScreen(window.Handle(),&wheel);
    SendMessageW(window.Handle(),WM_MOUSEWHEEL,MAKEWPARAM(0,WHEEL_DELTA),MAKELPARAM(wheel.x,wheel.y));
    Require(window.ConsumeInput().wheel==0,"Panel wheel leaked into camera");
    std::cout<<"PASS: Win32 + ImGui held/quick/repeated drag, same-owner capture, UI exclusion, release, focus/capture loss, no-UI\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
