#include "Core/AtomicFile.h"
#include "Core/Error.h"
#include <atomic>
namespace isr {
void AtomicWrite(const std::filesystem::path& path,std::span<const uint8_t> bytes,bool overwrite){
    static std::atomic_uint64_t serial=0;
    auto temporary=path;temporary+=L".pending-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(++serial);
    struct Pending {std::filesystem::path path;HANDLE handle=INVALID_HANDLE_VALUE;bool owned=false;~Pending(){if(handle!=INVALID_HANDLE_VALUE)CloseHandle(handle);if(owned){std::error_code ec;std::filesystem::remove(path,ec);}}} pending{temporary};
    pending.handle=CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    CheckWin32(pending.handle!=INVALID_HANDLE_VALUE);
    pending.owned=true;
    while(!bytes.empty()){const DWORD count=static_cast<DWORD>(std::min<size_t>(bytes.size(),1024*1024));DWORD written=0;
        CheckWin32(WriteFile(pending.handle,bytes.data(),count,&written,nullptr));if(written!=count)throw std::runtime_error("Short atomic file write");bytes=bytes.subspan(count);}
    CheckWin32(FlushFileBuffers(pending.handle));CheckWin32(CloseHandle(pending.handle));pending.handle=INVALID_HANDLE_VALUE;
    CheckWin32(MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_WRITE_THROUGH|(overwrite?MOVEFILE_REPLACE_EXISTING:0)));
}
}
