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
    void Recipe(HWND owner,bool save);
    void Reference(HWND owner){Choose(owner,4);}
    void Choose(HWND owner,int kind);
    bool Busy()const{return result_.valid();}
    std::optional<std::filesystem::path> Poll();
private:
    static UINT_PTR CALLBACK Hook(HWND,UINT,WPARAM,LPARAM);
    std::atomic<HWND> dialog_=nullptr;
    std::atomic<bool> closing_=false;
    std::future<std::filesystem::path> result_;
};
}
