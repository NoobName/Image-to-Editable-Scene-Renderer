#pragma once
#include <windows.h>
#include "App/InputState.h"
namespace isr {
bool HandleImGuiMessage(HWND,UINT,WPARAM,LPARAM);
void FilterImGuiInput(InputState&);
// ImGui owns the viewport window too. Route camera gestures by where they
// began, rather than treating WantCaptureMouse as a blanket veto.
class ViewportInput {
public:
    void SetRegion(RECT clientRect,bool hovered,bool blocked);
    bool HandleMessage(HWND,UINT,WPARAM,LPARAM);
    void Filter(InputState&) const;
private:
    bool Contains(POINT) const;
    RECT region_{};
    bool hovered_=false,blocked_=false,drag_=false,releasing_=false;
};
}
