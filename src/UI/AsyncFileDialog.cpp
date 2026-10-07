#include "UI/AsyncFileDialog.h"
#include "Core/Error.h"
#include <commdlg.h>
#include <objbase.h>
#include <array>
namespace isr {
AsyncFileDialog::~AsyncFileDialog(){closing_=true;if(auto window=dialog_.load())PostMessageW(window,WM_CLOSE,0,0);if(result_.valid())result_.wait();}
UINT_PTR CALLBACK AsyncFileDialog::Hook(HWND child,UINT message,WPARAM,LPARAM param){
    if(message==WM_INITDIALOG){auto& self=*reinterpret_cast<AsyncFileDialog*>(reinterpret_cast<OPENFILENAMEW*>(param)->lCustData);
        const auto window=GetParent(child);self.dialog_=window;if(self.closing_)PostMessageW(window,WM_CLOSE,0,0);}
    return 0;
}
void AsyncFileDialog::Open(HWND owner,bool python){Choose(owner,python?1:0);}
void AsyncFileDialog::Recipe(HWND owner,bool save){Choose(owner,save?3:2);}
void AsyncFileDialog::Choose(HWND owner,int kind){
    if(Busy())return;closing_=false;
    result_=std::async(std::launch::async,[this,owner,kind]{
        const bool python=kind==1;
        const HRESULT apartment=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);Check(apartment);
        struct Cleanup{AsyncFileDialog& self;~Cleanup(){self.dialog_=nullptr;CoUninitialize();}} cleanup{*this};
        std::array<wchar_t,32768> path{};OPENFILENAMEW request{};request.lStructSize=sizeof(request);request.hwndOwner=owner;
        request.lpstrFilter=python?L"Python interpreter (python.exe)\0*.exe\0\0":L"Images (*.jpg;*.jpeg;*.png)\0*.jpg;*.jpeg;*.png\0\0";
        request.lpstrFile=path.data();request.nMaxFile=static_cast<DWORD>(path.size());
        request.lpstrTitle=python?L"Select project environment python.exe":L"Reconstruct Image";
        request.Flags=OFN_EXPLORER|OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR|OFN_ENABLEHOOK;
        request.lpfnHook=Hook;request.lCustData=reinterpret_cast<LPARAM>(this);
        if(kind>=2){request.lpstrFilter=L"Relighting recipe (*.json)\0*.json\0\0";request.lpstrDefExt=L"json";request.lpstrTitle=L"Relighting recipe";}
        if(kind==4){request.lpstrFilter=L"Reference image or scene.json\0*.png;*.jpg;*.jpeg;*.json\0\0";request.lpstrTitle=L"Select reference image or saved ScenePackage scene.json";request.lpstrDefExt=nullptr;}
        if(kind==3){request.Flags&=~OFN_FILEMUSTEXIST;request.Flags|=OFN_OVERWRITEPROMPT;}
        if(kind==3?GetSaveFileNameW(&request):GetOpenFileNameW(&request))return std::filesystem::path(path.data());
        if(const auto error=CommDlgExtendedError())throw std::runtime_error("File chooser failed: "+std::to_string(error));
        return std::filesystem::path{};
    });
}
std::optional<std::filesystem::path> AsyncFileDialog::Poll(){
    if(!result_.valid()||result_.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return {};
    return result_.get();
}
}
