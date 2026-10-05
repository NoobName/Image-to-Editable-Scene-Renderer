#include "UI/ImGuiInput.h"
#include <imgui.h>
#include <backends/imgui_impl_win32.h>
#include <windowsx.h>
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND,UINT,WPARAM,LPARAM);
namespace isr {
bool HandleImGuiMessage(HWND window,UINT message,WPARAM wp,LPARAM lp){
    ImGui_ImplWin32_WndProcHandler(window,message,wp,lp);
    if(message==WM_SYSKEYDOWN&&wp==VK_F4)return false; // Leave Alt+F4 to DefWindowProc.
    const auto& io=ImGui::GetIO();
    return ((message==WM_KEYDOWN||message==WM_SYSKEYDOWN)&&io.WantCaptureKeyboard)||
        (message==WM_RBUTTONDOWN&&io.WantCaptureMouse);
}
void FilterImGuiInput(InputState& input){
    const auto& io=ImGui::GetIO();
    if(io.WantCaptureMouse){input.rightMouse=false;input.ClearDeltas();}
    if(io.WantCaptureKeyboard)input.keys.fill(false);
}
bool ViewportInput::Contains(POINT p) const {return PtInRect(&region_,p)!=FALSE;}
void ViewportInput::SetRegion(RECT rect,bool hovered,bool blocked){region_=rect;hovered_=hovered;blocked_=blocked;}
bool ViewportInput::HandleMessage(HWND window,UINT message,WPARAM wp,LPARAM lp){
    // ReleaseCapture in the backend sends a nested WM_CAPTURECHANGED.
    if(message==WM_RBUTTONUP)releasing_=true;
    if(message==WM_RBUTTONDOWN)drag_=!blocked_&&Contains({GET_X_LPARAM(lp),GET_Y_LPARAM(lp)});
    ImGui_ImplWin32_WndProcHandler(window,message,wp,lp);
    if(message==WM_RBUTTONUP){drag_=false;releasing_=false;}
    if((message==WM_CAPTURECHANGED&&reinterpret_cast<HWND>(lp)!=window&&!releasing_)||
       (message==WM_ACTIVATEAPP&&!wp))drag_=false;
    if(message==WM_RBUTTONDOWN)return !drag_;
    if(message==WM_SYSKEYDOWN&&wp==VK_F4)return false; // Native close works over panels too.
    if(message==WM_MOUSEWHEEL){
        POINT point{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};ScreenToClient(window,&point);
        return !drag_&&(blocked_||!Contains(point));
    }
    if(message==WM_KEYDOWN||message==WM_SYSKEYDOWN)
        return blocked_||ImGui::GetIO().WantTextInput||(!drag_&&!hovered_);
    return false;
}
void ViewportInput::Filter(InputState& input) const {
    if(!drag_)input.rightMouse=false;
    // Window produces deltas only for accepted RMB-down events. Keep the last
    // delta when a quick release ends outside the image in the same pump.
    if(blocked_||ImGui::GetIO().WantTextInput||(!hovered_&&!drag_))input.keys.fill(false);
}
}
