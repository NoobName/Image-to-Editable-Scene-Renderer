#include "App/Window.h"
#include "Core/Error.h"
#include <windowsx.h>
#include "Core/Log.h"
namespace isr {
namespace { constexpr wchar_t ClassName[] = L"ImageSceneRendererWindow"; }
Window::Window(HINSTANCE instance, uint32_t width, uint32_t height)
    : instance_(instance), width_(width), height_(height) {
    WNDCLASSEXW wc{sizeof(WNDCLASSEXW)};
    wc.lpfnWndProc = WndProc; wc.hInstance = instance; wc.lpszClassName = ClassName;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    if (!RegisterClassExW(&wc)) Check(HRESULT_FROM_WIN32(GetLastError()));
    RECT rect{0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
    CheckWin32(AdjustWindowRectEx(&rect, WS_OVERLAPPEDWINDOW, FALSE, 0));
    window_ = CreateWindowExW(0, ClassName, L"Image-to-Editable-Scene Renderer", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top,
        nullptr, nullptr, instance, this);
    if (!window_) { const auto error = GetLastError(); UnregisterClassW(ClassName, instance); Check(HRESULT_FROM_WIN32(error)); }
    ShowWindow(window_, SW_SHOW);
}
Window::~Window() {
    if (window_ && IsWindow(window_)) DestroyWindow(window_);
    UnregisterClassW(ClassName, instance_);
}
bool Window::Pump() {
    MSG message{};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
        if (message.message == WM_QUIT) return false;
        TranslateMessage(&message); DispatchMessageW(&message);
    }
    return true;
}
InputState Window::ConsumeInput() { const auto result = input_; input_.ClearDeltas(); return result; }
void Window::TestMinimizeRestore() {
    if (!SetTimer(window_,1,150,nullptr)) Check(HRESULT_FROM_WIN32(GetLastError()));
    ShowWindow(window_,SW_MINIMIZE); Log("Smoke: minimized");
}
void Window::SetClientSize(uint32_t width, uint32_t height) {
    RECT rect{0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
    CheckWin32(AdjustWindowRectEx(&rect, WS_OVERLAPPEDWINDOW, FALSE, 0));
    CheckWin32(SetWindowPos(window_, nullptr, 0, 0, rect.right - rect.left, rect.bottom - rect.top, SWP_NOMOVE | SWP_NOZORDER));
}
LRESULT CALLBACK Window::WndProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    auto* self = reinterpret_cast<Window*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        self = static_cast<Window*>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    return self ? self->HandleMessage(window, message, wparam, lparam) : DefWindowProcW(window, message, wparam, lparam);
}
LRESULT Window::HandleMessage(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    // ImGui's backend may call ReleaseCapture synchronously on button-up. Mark
    // a normal release before its callback so pending drag deltas survive it.
    if(message==WM_RBUTTONUP)input_.rightMouse=false;
    if(messageHandler_&&messageHandler_(window,message,wparam,lparam))return 0;
    switch (message) {
    case WM_TIMER:
        if (wparam == 1) { KillTimer(window,1); ShowWindow(window,SW_RESTORE); Log("Smoke: restored"); return 0; }
        break;
    case WM_SIZE:
        minimized_ = wparam == SIZE_MINIMIZED;
        width_ = LOWORD(lparam); height_ = HIWORD(lparam); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_ACTIVATEAPP:
        input_ = {}; input_.active = wparam != 0;
        if (!input_.active && GetCapture() == window) ReleaseCapture(); return 0;
    case WM_KEYDOWN:
        if (wparam == VK_ESCAPE) { DestroyWindow(window); return 0; }
        if (wparam < input_.keys.size()) input_.keys[wparam] = true; return 0;
    case WM_SYSKEYDOWN:
        if (wparam < input_.keys.size()) input_.keys[wparam] = true;
        if (wparam == VK_MENU) return 0; break;
    case WM_KEYUP: case WM_SYSKEYUP:
        if (wparam < input_.keys.size()) input_.keys[wparam] = false;
        if (wparam == VK_MENU) return 0; break;
    case WM_RBUTTONDOWN:
        // The ImGui Win32 backend may already own capture for this same HWND.
        // Reacquiring it generates WM_CAPTURECHANGED even though the owner stays
        // the same, which used to cancel every viewport right-button drag.
        if(GetCapture()!=window)SetCapture(window);
        input_.rightMouse = true; lastMouse_ = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};return 0;
    case WM_RBUTTONUP: input_.rightMouse = false; if (GetCapture() == window) ReleaseCapture(); return 0;
    case WM_CAPTURECHANGED:
        if(reinterpret_cast<HWND>(lparam)!=window){
            if(input_.rightMouse)input_.ClearDeltas(); // Actual loss interrupts the drag.
            input_.rightMouse=false;
        }
        return 0;
    case WM_MOUSEMOVE: {
        const POINT current{GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam)};
        if (input_.rightMouse) { input_.mouseX += float(current.x-lastMouse_.x); input_.mouseY += float(current.y-lastMouse_.y); }
        lastMouse_ = current; return 0;
    }
    case WM_MOUSEWHEEL: input_.wheel += float(GET_WHEEL_DELTA_WPARAM(wparam))/WHEEL_DELTA; return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}
}
