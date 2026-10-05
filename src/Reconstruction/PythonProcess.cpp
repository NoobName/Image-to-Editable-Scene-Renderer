#include "Reconstruction/PythonProcess.h"
#include "Core/Error.h"
#include <map>
#include <cwchar>
namespace isr {
std::wstring QuoteProcessArgument(const std::wstring& argument) {
    std::wstring result=L"\"";size_t slashes=0;
    for(wchar_t c:argument) {
        if(c==L'\\'){++slashes;continue;}
        result.append(c==L'"'?slashes*2+1:slashes,L'\\');slashes=0;result+=c;
    }
    result.append(slashes*2,L'\\');result+=L'"';return result;
}
namespace {
struct IgnoreCase { bool operator()(const std::wstring& a,const std::wstring& b)const{return _wcsicmp(a.c_str(),b.c_str())<0;} };
std::vector<wchar_t> Environment(const std::filesystem::path& python) {
    auto* block=GetEnvironmentStringsW();if(!block)Check(HRESULT_FROM_WIN32(GetLastError()));
    struct Release {wchar_t* data;~Release(){FreeEnvironmentStringsW(data);}} release{block};
    std::map<std::wstring,std::wstring,IgnoreCase> values;
    for(auto* p=block;*p;p+=wcslen(p)+1){std::wstring entry=p;auto split=entry.find(L'=',entry[0]==L'='?1:0);
        if(split!=std::wstring::npos)values[entry.substr(0,split)]=entry.substr(split+1);}
    const auto root=python.parent_path();
    values[L"PATH"]=root.wstring()+L";"+(root/L"Scripts").wstring()+L";"+(root/L"Library/bin").wstring()+L";"+values[L"PATH"];
    values[L"PYTHONUTF8"]=L"1";values[L"PYTHONUNBUFFERED"]=L"1";
    // Do not let an unrelated activated environment inject modules into the selected interpreter.
    values.erase(L"PYTHONHOME");values.erase(L"PYTHONPATH");
    std::vector<wchar_t> result;
    for(const auto& [key,value]:values){auto entry=key+L"="+value;result.insert(result.end(),entry.begin(),entry.end());result.push_back(0);}
    result.push_back(0);return result;
}
}
unsigned RunPythonProcess(const std::filesystem::path& python,const std::vector<std::wstring>& arguments,
    const std::filesystem::path& directory,const std::filesystem::path& log,std::stop_token stop,const std::function<void()>& poll) {
    SECURITY_ATTRIBUTES security{sizeof(security),nullptr,TRUE};
    UniqueHandle output(CreateFileW(log.c_str(),GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr));
    if(output.Get()==INVALID_HANDLE_VALUE)Check(HRESULT_FROM_WIN32(GetLastError()));
    CheckWin32(SetHandleInformation(output.Get(),HANDLE_FLAG_INHERIT,HANDLE_FLAG_INHERIT));
    UniqueHandle input(CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&security,OPEN_EXISTING,0,nullptr));
    if(input.Get()==INVALID_HANDLE_VALUE)Check(HRESULT_FROM_WIN32(GetLastError()));
    UniqueHandle job(CreateJobObjectW(nullptr,nullptr));if(!job.Get())Check(HRESULT_FROM_WIN32(GetLastError()));
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    CheckWin32(SetInformationJobObject(job.Get(),JobObjectExtendedLimitInformation,&limits,sizeof(limits)));
    SIZE_T bytes=0;InitializeProcThreadAttributeList(nullptr,2,0,&bytes);
    if(GetLastError()!=ERROR_INSUFFICIENT_BUFFER)Check(HRESULT_FROM_WIN32(GetLastError()));
    std::vector<unsigned char> storage(bytes);
    auto* attributes=reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage.data());
    CheckWin32(InitializeProcThreadAttributeList(attributes,2,0,&bytes));
    struct Cleanup {LPPROC_THREAD_ATTRIBUTE_LIST value;~Cleanup(){DeleteProcThreadAttributeList(value);}} cleanup{attributes};
    HANDLE inherited[]{input.Get(),output.Get()},jobHandle=job.Get();
    CheckWin32(UpdateProcThreadAttribute(attributes,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,inherited,sizeof(inherited),nullptr,nullptr));
    CheckWin32(UpdateProcThreadAttribute(attributes,0,PROC_THREAD_ATTRIBUTE_JOB_LIST,&jobHandle,sizeof(jobHandle),nullptr,nullptr));
    STARTUPINFOEXW startup{};startup.StartupInfo.cb=sizeof(startup);startup.lpAttributeList=attributes;
    startup.StartupInfo.dwFlags=STARTF_USESTDHANDLES;startup.StartupInfo.hStdInput=input.Get();
    startup.StartupInfo.hStdOutput=startup.StartupInfo.hStdError=output.Get();
    std::wstring command=QuoteProcessArgument(python.wstring());
    for(const auto& argument:arguments)command+=L" "+QuoteProcessArgument(argument);
    auto environment=Environment(python);PROCESS_INFORMATION process{};
    CheckWin32(CreateProcessW(python.c_str(),command.data(),nullptr,nullptr,TRUE,
        CREATE_NO_WINDOW|CREATE_UNICODE_ENVIRONMENT|EXTENDED_STARTUPINFO_PRESENT,environment.data(),directory.c_str(),&startup.StartupInfo,&process));
    UniqueHandle processHandle(process.hProcess),threadHandle(process.hThread);
    for(;;){
        if(stop.stop_requested()) {CheckWin32(TerminateJobObject(job.Get(),ERROR_CANCELLED));
            const auto wait=WaitForSingleObject(processHandle.Get(),5000);
            if(wait==WAIT_FAILED)Check(HRESULT_FROM_WIN32(GetLastError()));
            throw std::runtime_error("Reconstruction cancelled; the current scene was kept.");}
        const DWORD wait=WaitForSingleObject(processHandle.Get(),100);
        if(wait==WAIT_FAILED)Check(HRESULT_FROM_WIN32(GetLastError()));
        poll();
        if(wait==WAIT_OBJECT_0)break;
        if(wait!=WAIT_TIMEOUT)throw std::runtime_error("Unexpected Python process wait result");
    }
    DWORD code=0;CheckWin32(GetExitCodeProcess(processHandle.Get(),&code));return code;
}
}
