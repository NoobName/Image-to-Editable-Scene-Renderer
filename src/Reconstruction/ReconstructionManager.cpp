#include "Reconstruction/ReconstructionManager.h"
#include "Reconstruction/PythonProcess.h"
#include "Assets/AssetIO.h"
#include "Core/Log.h"
#include "Core/Error.h"
#include <nlohmann/json.hpp>
#include <windows.h>
#include <fstream>
#include <chrono>
#include <algorithm>
namespace isr {
namespace {
using Json=nlohmann::json;
std::filesystem::path Utf8Path(const std::string& text){return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(text.data()),text.size()));}
std::wstring EnvironmentValue(const wchar_t* name){
    const DWORD size=GetEnvironmentVariableW(name,nullptr,0);if(!size)return {};
    std::wstring result(size,L'\0');const auto written=GetEnvironmentVariableW(name,result.data(),size);
    if(!written||written>=size)return {};result.resize(written);return result;
}
std::filesystem::path DefaultPython(const std::filesystem::path& root) {
    if(const auto configured=EnvironmentValue(L"ISR_PYTHON");!configured.empty())return configured;
    if(const auto profile=EnvironmentValue(L"USERPROFILE");!profile.empty()){
        std::ifstream environments(std::filesystem::path(profile)/".conda/environments.txt");std::string line;
        while(std::getline(environments,line)) {if(!line.empty()&&line.back()=='\r')line.pop_back();auto env=Utf8Path(line);
            if(env.filename()=="image-scene-renderer"&&std::filesystem::is_regular_file(env/"python.exe"))return env/"python.exe";}
    }
    const auto local=root/"tools/reconstruction/.venv/Scripts/python.exe";
    if(std::filesystem::is_regular_file(local))return local;
    return {};
}
std::string Tail(const std::filesystem::path& path){
    std::ifstream file(path,std::ios::binary);if(!file)return {};
    file.seekg(0,std::ios::end);auto size=file.tellg();if(size>8192)file.seekg(size-std::streamoff(8192));else file.seekg(0);
    return {std::istreambuf_iterator<char>(file),{}};
}
}
const char* ReconstructionStateName(ReconstructionState state){
    switch(state){case ReconstructionState::Idle:return "Idle";case ReconstructionState::Processing:return "Processing";
    case ReconstructionState::Loading:return "Loading";case ReconstructionState::Ready:return "Ready";case ReconstructionState::Error:return "Error";}return "Error";
}
std::filesystem::path ReconstructionManager::FindProjectRoot(const std::filesystem::path& executableDirectory){
    for(auto start:{executableDirectory,std::filesystem::current_path()})for(auto path=start;!path.empty();path=path.parent_path()){
        if(std::filesystem::is_regular_file(path/"tools/reconstruction/reconstruct.py"))return path;
        if(path==path.parent_path())break;
    }
    return std::filesystem::current_path();
}
ReconstructionManager::ReconstructionManager(std::filesystem::path root) {
    options.projectRoot=std::move(root);options.python=DefaultPython(options.projectRoot);
    try {std::ifstream file(options.projectRoot/"generated/reconstruction-settings.json");if(file){auto data=Json::parse(file);
        auto path=Utf8Path(data.at("python").get<std::string>());if(std::filesystem::is_regular_file(path))options.python=path;}}
    catch(const std::exception& e){Log(std::string("Reconstruction settings ignored: ")+e.what());}
    if(const auto configured=EnvironmentValue(L"ISR_PYTHON");!configured.empty())options.python=configured;
}
ReconstructionManager::~ReconstructionManager(){Cancel();if(worker_.joinable())worker_.join();}
ReconstructionStatus ReconstructionManager::Status()const{std::lock_guard lock(mutex_);return status_;}
bool ReconstructionManager::Start(const std::filesystem::path& input,bool lightingOnly,bool intrinsicOnly,bool shadowOnly){
    if(int(lightingOnly)+int(intrinsicOnly)+int(shadowOnly)>1)throw std::invalid_argument("Choose one offline analysis stage");
    if(Status().Busy())return false;
    if(worker_.joinable())worker_.join(); // Terminal status is published at the end of worker execution.
    {std::lock_guard lock(mutex_);status_={};status_.state=ReconstructionState::Processing;status_.input=input;
        status_.message="Starting Python pipeline";sequence_=0;package_.reset();}
    auto config=options;config.lightingOnly=lightingOnly;config.intrinsicOnly=intrinsicOnly;config.shadowOnly=shadowOnly;lastOptions_=config;
    {std::lock_guard lock(mutex_);status_.lightingOnly=lightingOnly;status_.intrinsicOnly=intrinsicOnly;status_.shadowOnly=shadowOnly;if(lightingOnly||intrinsicOnly||shadowOnly){status_.stageNames={shadowOnly?"shadow":intrinsicOnly?"intrinsic":"lighting","export"};status_.stages={"pending","pending"};}}
    try{worker_=std::jthread([this,config,input](std::stop_token stop){Run(stop,config,input);});}
    catch(const std::exception& e){Fail(e.what());return false;}
    return true;
}
void ReconstructionManager::Cancel(){
    {std::lock_guard lock(mutex_);if(!status_.Busy())return;status_.cancelRequested=true;status_.message="Cancelling reconstruction...";}
    worker_.request_stop();
}
bool ReconstructionManager::Retry(){
    const auto status=Status();const auto previous=options;
    if(status.lightingOnly){options.lighting=lastOptions_.lighting;options.calibration=lastOptions_.calibration;}
    if(status.intrinsicOnly){options.intrinsic=lastOptions_.intrinsic;options.maxSize=lastOptions_.maxSize;options.offline=lastOptions_.offline;}
    const auto started=Start(status.input,status.lightingOnly,status.intrinsicOnly,status.shadowOnly);options=previous;return started;
}
void ReconstructionManager::Fail(std::string message){
    std::lock_guard lock(mutex_);status_.state=ReconstructionState::Error;status_.message=std::move(message);package_.reset();
    Log("Reconstruction Error: "+status_.message);
}
void ReconstructionManager::Ready(){std::lock_guard lock(mutex_);status_.state=ReconstructionState::Ready;status_.message="Scene loaded. Materials and objects are editable.";Log("Reconstruction Ready: "+PathUtf8(status_.output));}
std::unique_ptr<ScenePackage> ReconstructionManager::TakePackage(){std::lock_guard lock(mutex_);return std::move(package_);}
void ReconstructionManager::ReadProgress(const std::filesystem::path& path,bool final){
    try {
        std::string bytes;
        { // FILE_SHARE_DELETE is required for Python's atomic os.replace on Windows.
            UniqueHandle file(CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
                nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr));
            if(file.Get()==INVALID_HANDLE_VALUE)throw std::runtime_error("Python did not publish a readable progress snapshot");
            LARGE_INTEGER size{};CheckWin32(GetFileSizeEx(file.Get(),&size));
            if(size.QuadPart<0||size.QuadPart>65536)throw std::runtime_error("Progress snapshot is too large");
            bytes.resize(static_cast<size_t>(size.QuadPart));DWORD read=0;
            CheckWin32(ReadFile(file.Get(),bytes.data(),static_cast<DWORD>(bytes.size()),&read,nullptr));bytes.resize(read);
        }
        const auto data=Json::parse(bytes);std::lock_guard lock(mutex_);
        if(data.at("job_id")!=status_.jobId)throw std::runtime_error("Progress identity mismatch");
        auto [names,states]=ParseProgressTable(data); // Validate the complete snapshot before publishing any field.
        const auto sequence=data.at("sequence").get<uint64_t>();
        if(sequence>=sequence_){
            for(size_t i=0;i<names.size();++i)if(status_.stageNames!=names||status_.stages[i]!=states[i])Log("Reconstruction stage "+names[i]+": "+states[i]);
            status_.stageNames=std::move(names);status_.stages=std::move(states);
            sequence_=sequence;if(!status_.cancelRequested)status_.message=data.at("detail").get<std::string>();
        }
        if(final&&(data.at("state")!="complete"||std::any_of(status_.stages.begin(),status_.stages.end(),[](const auto& s){return s!="complete";})))
            throw std::runtime_error("Python exited without completing all reconstruction stages");
    }catch(...){if(final)throw;} // Atomic replacement can momentarily contend with antivirus/readers; next poll retries.
}
void ReconstructionManager::Run(std::stop_token stop,ReconstructionOptions config,std::filesystem::path input){
    try {
        const auto root=std::filesystem::canonical(config.projectRoot);
        if(config.python.empty()||!std::filesystem::is_regular_file(config.python))throw std::runtime_error("Select an existing project python.exe in File > Reconstruction settings.");
        const auto python=std::filesystem::canonical(config.python);input=std::filesystem::canonical(input);
        if(!std::filesystem::is_regular_file(python)||python.extension()!=L".exe")throw std::runtime_error("Select the project environment's python.exe in Reconstruction settings.");
        const auto script=root/"tools/reconstruction"/(config.shadowOnly?"estimate_shadows.py":config.intrinsicOnly?"estimate_intrinsic.py":config.lightingOnly?"estimate_lighting.py":"reconstruct.py");
        if(!std::filesystem::is_regular_file(script))throw std::runtime_error("Project tools/reconstruction/reconstruct.py was not found.");
        if(config.maxSize<16||config.maxSize>2048)throw std::runtime_error("Image size must be 16..2048");
        if((config.geometry!="moge"&&config.geometry!="dummy")||(config.segmentation!="sam2"&&config.segmentation!="dummy")||
            (config.materials!="marigold"&&config.materials!="neutral"))throw std::runtime_error("Unknown reconstruction backend");
        const auto id=std::to_string(GetCurrentProcessId())+"-"+std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        auto stem=input.stem().wstring();for(auto& c:stem)if(c<32||std::wstring_view(L"<>:\"/\\|?*").find(c)!=std::wstring_view::npos)c=L'_';
        if(stem.size()>64)stem.resize(64);if(stem.empty())stem=L"scene";
        const auto generated=root/"generated",output=generated/(stem+L"-"+std::wstring(id.begin(),id.end()));
        const auto job=generated/".reconstruction-jobs"/id,progress=job/"progress.json",log=job/"python.log";
        std::filesystem::create_directories(job);
        {std::lock_guard lock(mutex_);status_.jobId=id;status_.input=input;status_.output=output;status_.log=log;}
        {std::ofstream settings(generated/"reconstruction-settings.json");settings<<Json{{"python",PathUtf8(python)}}.dump(2);}
        auto wide=[](const std::string& value){return std::wstring(value.begin(),value.end());};
        std::vector<std::wstring> arguments{L"-u",script.wstring(),input.wstring(),L"--output",output.wstring(),
            L"--progress-file",progress.wstring(),L"--job-id",wide(id),L"--geometry-backend",wide(config.geometry),
            L"--segmentation-backend",wide(config.segmentation),L"--material-backend",wide(config.materials),
            L"--device",L"auto",L"--max-size",std::to_wstring(config.maxSize)};
        if(config.lightingOnly||config.intrinsicOnly||config.shadowOnly)arguments={L"-u",script.wstring(),input.wstring(),L"--output",output.wstring(),L"--progress-file",progress.wstring(),L"--job-id",wide(id)};
        else if(config.offline)arguments.push_back(L"--offline");
        if(config.intrinsicOnly){
            if(config.intrinsic!="marigold-lighting"&&config.intrinsic!="proxy"&&config.intrinsic!="saved")throw std::runtime_error("Unknown intrinsic backend");
            arguments.insert(arguments.end(),{L"--backend",wide(config.intrinsic),L"--resolution",std::to_wstring(config.maxSize)});
            if(config.offline)arguments.push_back(L"--offline");
        }else if(!config.shadowOnly)arguments.insert(arguments.end(),{L"--lighting-backend",wide(config.lighting)});
        if(!config.shadowOnly&&!config.intrinsicOnly&&config.lighting=="manual-test"){
            auto values=[&](const wchar_t* key,const std::array<float,3>& rgb,float gain){arguments.emplace_back(key);for(float v:rgb)arguments.push_back(std::to_wstring(v*gain));};
            values(L"--light-direction",config.calibration.direction,1);values(L"--direct-rgb",config.calibration.directColor,config.calibration.directIntensity);
            values(L"--ambient-rgb",config.calibration.ambientColor,config.calibration.ambientIntensity);
        }
        Log("Reconstruction Processing: "+PathUtf8(input)+" -> "+PathUtf8(output));
        const auto exitCode=RunPythonProcess(python,arguments,root,log,stop,[&]{ReadProgress(progress);});
        if(exitCode)throw std::runtime_error("Python exited with code "+std::to_string(exitCode)+".\n"+Tail(log));
        ReadProgress(progress,true);
        if(stop.stop_requested())throw std::runtime_error("Reconstruction cancelled; current scene retained.");
        {std::lock_guard lock(mutex_);status_.state=ReconstructionState::Loading;status_.message="Loading ScenePackage and decoding textures...";}
        Log("Reconstruction Loading: "+PathUtf8(output));
        auto loaded=std::make_unique<ScenePackage>(ScenePackageLoader{}.Load(output));
        if(stop.stop_requested())throw std::runtime_error("Reconstruction cancelled; current scene retained.");
        {std::lock_guard lock(mutex_);package_=std::move(loaded);status_.message="Preparing GPU scene...";}
    }catch(const std::exception& e){Fail(e.what());}
}
}
