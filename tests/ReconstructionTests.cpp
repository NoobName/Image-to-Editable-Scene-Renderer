#include "Reconstruction/PythonProcess.h"
#include "Reconstruction/ReconstructionManager.h"
#include "Core/Error.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <future>
#include <thread>
using namespace isr;
namespace {
void Require(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
bool Running(DWORD pid){UniqueHandle process(OpenProcess(SYNCHRONIZE,FALSE,pid));return process.Get()&&WaitForSingleObject(process.Get(),0)==WAIT_TIMEOUT;}
}
int wmain(int argc,wchar_t** argv){try{
    if(argc!=5)throw std::runtime_error("Pass Python, process fixture, package manifest and damaged-sidecar fixture paths");
    const std::filesystem::path python=argv[1],fixture=argv[2];
    const auto root=std::filesystem::temp_directory_path()/(L"ISR process tests "+std::to_wstring(GetCurrentProcessId()));
    std::filesystem::create_directories(root);
    struct Cleanup{std::filesystem::path root;~Cleanup(){std::error_code error;std::filesystem::remove_all(root,error);}}cleanup{root};
    const auto result=root/L"参数.json",log=root/"python.log";
    std::vector<std::wstring> values{L"",L"spaces and 中文",L"a\"b",L"trailing\\",L"&whoami|echo > do-not-create",L"%PATH%",L"two\\\\\"quotes"};
    std::vector<std::wstring> arguments{fixture.wstring(),L"arguments",result.wstring()};arguments.insert(arguments.end(),values.begin(),values.end());
    int polls=0;
    Require(RunPythonProcess(python,arguments,root,log,{},[&]{++polls;})==0,"Process failed");
    const auto decoded=nlohmann::json::parse(std::ifstream(result));
    for(size_t i=0;i<values.size();++i){const auto encoded=std::filesystem::path(values[i]).u8string();
        Require(decoded[i]==std::string(reinterpret_cast<const char*>(encoded.data()),encoded.size()),"Unicode/quote/backslash argument corruption");}
    Require(polls>0&&std::filesystem::file_size(log)>300000,"Redirected stdout deadlocked or was lost");
    Require(RunPythonProcess(python,{fixture.wstring(),L"failure",result.wstring()},root,log,{},[]{})==7,"Exit status was lost");
    const auto tree=root/"tree.json";std::stop_source stop;
    auto worker=std::async(std::launch::async,[&]{try{RunPythonProcess(python,{fixture.wstring(),L"tree",tree.wstring()},root,log,stop.get_token(),[]{});return false;}catch(const std::exception&){return true;}});
    for(unsigned i=0;i<100&&!std::filesystem::exists(tree);++i)std::this_thread::sleep_for(std::chrono::milliseconds(50));
    Require(std::filesystem::exists(tree),"Child process tree did not start");
    const auto pids=nlohmann::json::parse(std::ifstream(tree));stop.request_stop();
    Require(worker.wait_for(std::chrono::seconds(6))==std::future_status::ready&&worker.get(),"Cancellation did not finish");
    for(unsigned i=0;i<50&&Running(pids["child"].get<DWORD>());++i)std::this_thread::sleep_for(std::chrono::milliseconds(20));
    Require(!Running(pids["parent"].get<DWORD>())&&!Running(pids["child"].get<DWORD>()),"Cancellation left an orphan process");
    ReconstructionManager manager(root);manager.options.python=root/"missing.exe";
    Require(manager.Start(result),"First job did not start");
    for(unsigned i=0;i<100&&manager.Status().Busy();++i)std::this_thread::sleep_for(std::chrono::milliseconds(10));
    Require(manager.Status().state==ReconstructionState::Error&&!manager.TakePackage(),"Missing interpreter did not become recoverable Error");
    Require(manager.Start(result),"Retry after failure was blocked");
    std::filesystem::create_directories(root/"tools/reconstruction");
    std::filesystem::copy_file(argv[4],root/"tools/reconstruction/reconstruct.py");
    ReconstructionManager damaged(root);damaged.options.python=python;
    Require(damaged.Start(argv[3]),"Damaged-package job did not start");
    for(unsigned i=0;i<2000&&damaged.Status().Busy();++i)std::this_thread::sleep_for(std::chrono::milliseconds(10));
    Require(damaged.Status().state==ReconstructionState::Error&&!damaged.TakePackage(),"Invalid sidecar was handed to GPU scene replacement");
    Require(damaged.Status().message.find("relighting/relighting.json")!=std::string::npos,"Invalid extension lacked actionable diagnosis");
    std::cout<<"Argument quoting, Unicode, log back-pressure, exit status, process-tree cancellation and retry: OK\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
