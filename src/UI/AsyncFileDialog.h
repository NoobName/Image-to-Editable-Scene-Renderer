#pragma once
#include <windows.h>
#include <filesystem>
#include <future>
#include <atomic>
#include <optional>
namespace isr {
class AsyncFileDialog {
public:
    ~AsyncFileDialog();
    void Open(HWND owner,bool python=false);
    bool Busy()const{return result_.valid();}
    std::optional<std::filesystem::path> Poll();
private:
    static UINT_PTR CALLBACK Hook(HWND,UINT,WPARAM,LPARAM);
    std::atomic<HWND> dialog_=nullptr;
    std::atomic<bool> closing_=false;
    std::future<std::filesystem::path> result_;
};
}
