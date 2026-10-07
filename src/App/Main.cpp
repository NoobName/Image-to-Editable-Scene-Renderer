#include "App/Application.h"
#include "Core/Error.h"
#include "Core/Log.h"
#include <shellapi.h>
#include <memory>
#include <string>
namespace {
std::wstring ErrorText(const char* text){
    UINT codePage=CP_UTF8;DWORD flags=MB_ERR_INVALID_CHARS;
    int size=MultiByteToWideChar(codePage,flags,text,-1,nullptr,0);
    if(!size){codePage=CP_ACP;flags=0;size=MultiByteToWideChar(codePage,flags,text,-1,nullptr,0);}
    if(!size)return L"未知程序错误";
    std::wstring result(size,L'\0');MultiByteToWideChar(codePage,flags,text,-1,result.data(),size);result.pop_back();return result;
}
}
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    bool unattended=false;
    try {
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        int argc = 0;
        auto deleter = [](wchar_t** value) { LocalFree(value); };
        std::unique_ptr<wchar_t*, decltype(deleter)> argv(CommandLineToArgvW(GetCommandLineW(), &argc), deleter);
        if (!argv) isr::Check(HRESULT_FROM_WIN32(GetLastError()));
        for(int i=1;i<argc;++i)if(std::wstring_view(argv.get()[i])==L"--smoke"||std::wstring_view(argv.get()[i])==L"--frames")unattended=true;
        return isr::RunApplication(instance, argc, argv.get());
    } catch (const std::exception& e) {
        isr::Log(std::string("FATAL: ") + e.what());
        // Interactive failures must be visible; finite smoke runs must not hang
        // waiting for a dialog. Both paths keep the full error in the log.
        if(!unattended){
            auto message=ErrorText(e.what());const auto log=isr::LogPath();
            if(!log.empty())message+=L"\n\n日志："+log.wstring();
            // A Window destroyed while unwinding posts WM_QUIT. Remove that
            // stale quit before starting MessageBox's modal message loop.
            MSG quit{};while(PeekMessageW(&quit,nullptr,WM_QUIT,WM_QUIT,PM_REMOVE)){}
            MessageBoxW(nullptr,message.c_str(),L"图像到可编辑场景渲染器 - 错误",MB_OK|MB_ICONERROR);
        }
        return 1;
    }
}
