#pragma once
#include <windows.h>
#include <cstdint>
#include <functional>
#include "App/InputState.h"
namespace isr {
class Window {
public:
    Window(HINSTANCE instance, uint32_t width, uint32_t height);
    ~Window();
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    bool Pump();
    HWND Handle() const { return window_; }
    uint32_t Width() const { return width_; }
    uint32_t Height() const { return height_; }
    bool Minimized() const { return minimized_; }
    void SetClientSize(uint32_t width, uint32_t height);
    void TestMinimizeRestore();
    InputState ConsumeInput();
    void SetMessageHandler(std::function<bool(HWND,UINT,WPARAM,LPARAM)> handler){messageHandler_=std::move(handler);}
private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT HandleMessage(HWND, UINT, WPARAM, LPARAM);
    HINSTANCE instance_{};
    HWND window_{};
    uint32_t width_{}, height_{};
    bool minimized_{};
    InputState input_;
    POINT lastMouse_{};
    std::function<bool(HWND,UINT,WPARAM,LPARAM)> messageHandler_;
};
}
