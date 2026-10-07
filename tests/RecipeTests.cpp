#include "ScenePackage/RelightingRecipe.h"
#include "ScenePackage/AnchorImage.h"
#include "Assets/AssetIO.h"
#include "Assets/ImageExport.h"
#include "Assets/ImageDecoder.h"
#include "Core/AtomicFile.h"
#include "Core/Error.h"
#include <cstring>
#include <objbase.h>
#include <iostream>
#include <functional>
using namespace isr;
namespace {
void Need(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
void Reject(const std::function<void()>& fn){bool failed=false;try{fn();}catch(const std::exception&){failed=true;}Need(failed,"Expected rejection");}
void JsonFile(const std::filesystem::path& path,const package::Json& j){const auto text=j.dump();AtomicWrite(path,std::span(reinterpret_cast<const uint8_t*>(text.data()),text.size()),true);}
}
int wmain(int argc,wchar_t** argv){try{
    if(argc==1){
        ValidateNativeExportSize(1500,1000);ValidateNativeExportSize(3840,2160);
        Reject([]{ValidateNativeExportSize(8193,1);});Reject([]{ValidateNativeExportSize(4096,4096);});Reject([]{ValidateNativeExportSize(0,1);});
        auto source=std::make_shared<SourceObservation>();source->anchor.emplace();source->anchor->pixels=std::make_shared<ImageData>();source->regions.push_back({7,"stable-id","test"});
        RelightingSession s;s.Publish(source);s.protection.Apply(7,.8f);s.lighting.targetGlobalGain=1.3f;
        s.fog.enabled=true;s.fog.density=.7f;s.fog.allowRelativeScale=true;s.fog.airlight={.1f,.4f,.9f};
        const auto good=RecipeState(s);RelightingSession restored;restored.Publish(source);ApplyRecipeState(restored,good);Need(RecipeState(restored)==good,"CPU state roundtrip");
        auto legacy=good;legacy.erase("imageFog");ApplyRecipeState(restored,legacy);Need(restored.fog==ImageFogParameters{},"Old recipe must reset atmosphere");
        for(int i=0;i<5;++i){auto bad=good;if(i==0)bad["imageFog"]["density"]=-1;if(i==1)bad["imageFog"]["density"]=1001;
            if(i==2)bad["imageFog"]["airlightLinear"][0]=2;if(i==3)bad["imageFog"]["sourceAdditionalDensity"]=1;if(i==4)bad["imageFog"]["version"]=2;
            Reject([&]{ApplyRecipeState(s,bad);});Need(RecipeState(s)==good,"Fog validation must be atomic");}
        for(int i=0;i<5;++i){auto bad=good;if(i==0)bad["targetGlobalGain"]=-1;if(i==1)bad["target"]["direction"]={0,0,0};if(i==2)bad["response"]["epsilon"]=0;if(i==3)bad["protectedRegions"][0]["id"]="wrong";if(i==4)bad["unknown"]=0;
            Reject([&]{ApplyRecipeState(s,bad);});Need(RecipeState(s)==good,"Failure atomicity");}
        std::cout<<"Recipe state, bounds, stable ID and failure atomicity passed\n";return 0;
    }
    Need(argc==3,"RecipeTests fixture output");Check(CoInitializeEx(nullptr,COINIT_MULTITHREADED));
    const std::filesystem::path out=argv[2];std::filesystem::create_directories(out);
    const auto root=out/(L"配方试验-"+std::to_wstring(GetCurrentProcessId()));Need(!std::filesystem::exists(root),"Unique test directory");std::filesystem::create_directory(root);
    std::filesystem::copy(argv[1],root/L"原包",std::filesystem::copy_options::recursive);
    auto loaded=ScenePackageLoader{}.Load(root/L"原包");RelightingSession s;s.Publish(loaded.observation);s.SetMode(WorkMode::ImageRelighting);
    s.lighting.target.direction[0]*=-1;s.lighting.targetGlobalGain=1.7f;s.display.exposure=.6f;s.display.lowConfidence=true;s.relighting.specularRoughnessScale=1.3f;s.relighting.shadowStrength=.82f;
    s.fog.enabled=true;s.fog.density=.37f;s.fog.allowRelativeScale=true;
    Need(!s.Source()->regions.empty(),"Region fixture required");s.protection.Apply(s.Source()->regions.front().label,.75f);
    const auto originalManifest=ReadAssetFile(root/L"原包/scene.json");const auto path=root/L"测试配方.json";SaveRecipe(path,s,{});
    const auto good=ReadAssetFile(path);auto restored=LoadRecipe(path);Need(RecipeState(restored.session)==RecipeState(s),"Full state round trip");
    Reject([&]{SaveRecipe(path,s,{});});Need(good==ReadAssetFile(path),"New-file failure preserves previous bytes");
    Reject([&]{SaveRecipe(root/L"原包/scene.json",s,{},true);});Need(originalManifest==ReadAssetFile(root/L"原包/scene.json"),"Immutable package");
    s.lighting.draft=s.lighting.source;s.lighting.draft.direction={0,0,1};Need(s.lighting.ApplySource(),"Manual calibration");SaveRecipe(path,s,{},true);
    restored=LoadRecipe(path);Need(restored.session.lighting.manualSource&&!restored.session.lighting.cacheValid&&restored.session.lighting.sourceRevision==1,"Calibration revision/cache");
    Need(RecipeState(restored.session)==RecipeState(s),"Manual baseline and target independent");
    const auto valid=package::ReadJson(path);
    for(int test=0;test<8;++test){auto j=valid;
        if(test==0)j["version"]=99;if(test==1)j["identity"]["anchorSha256"]=std::string(64,'0');
        if(test==2)j["sourcePackage"]="missing";if(test==3)j["state"]["displayExposure"]=100;
        if(test==4)j["state"]["target"]["direction"]={0,0,0};if(test==5)j["state"]["protectedRegions"][0]["id"]="not-the-region";
        if(test==6)j["state"]["response"]["epsilon"]=nullptr;if(test==7)j["unknownField"]=true;
        JsonFile(root/L"损坏.json",j);Reject([&]{LoadRecipe(root/L"损坏.json");});}
    const auto before=RecipeState(s);auto bad=before;bad["targetGlobalGain"]=-1;Reject([&]{ApplyRecipeState(s,bad);});Need(before==RecipeState(s),"Failed apply preserves complete session");
    // Lock the destination against deletion: failed explicit overwrite must keep the old file.
    HANDLE lock=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);Need(lock!=INVALID_HANDLE_VALUE,"Lock file");
    Reject([&]{SaveRecipe(path,s,{},true);});CloseHandle(lock);Need(package::ReadJson(path)==valid,"Write failure preserves file");
    const auto moved=out/(L"移动后-"+std::to_wstring(GetCurrentProcessId()));std::filesystem::rename(root,moved);
    restored=LoadRecipe(moved/L"测试配方.json");Need(RecipeState(restored.session)==before,"Portable sibling paths");
    NumericImage image{3,5,NumericFormat::Float,{}};image.bytes.resize(3*5*4);for(size_t i=0;i<15;++i){float f=float(i)/15;std::memcpy(image.bytes.data()+i*4,&f,4);}
    WriteNumericDds(moved/"odd.dds",image);Need(LoadNumericDds(moved/"odd.dds").bytes==image.bytes,"Controlled DDS round trip");
    WriteRgbPng(moved/"original.png",*s.Source()->anchor->pixels);
    Need(DecodeImage(ReadAssetFile(moved/"original.png"),"PNG check")->rgba==s.Source()->anchor->pixels->rgba,"PNG RGB channel/transfer identity");
    ImageData mask;mask.width=3;mask.height=5;mask.rgba.assign(3*5*4,128);for(size_t i=3;i<mask.rgba.size();i+=4)mask.rgba[i]=255;
    WriteRgbPng(moved/"user-mask.png",mask);auto imported=LoadProtectionMask(moved/"user-mask.png");
    SaveRecipe(moved/L"保护配方.json",restored.session,imported);
    std::filesystem::rename(moved/"user-mask.png",moved/"user-mask-original-renamed.png");
    const auto masked=LoadRecipe(moved/L"保护配方.json");Need(masked.importedMask->image.bytes==imported->image.bytes,"Recipe owns portable mask copy");
    const auto record=masked.document.at("importedMask").at("path").get<std::string>();const auto maskPath=moved/std::filesystem::path(record);
    std::filesystem::rename(maskPath,moved/"temporarily-missing-mask.png");Reject([&]{LoadRecipe(moved/L"保护配方.json");});std::filesystem::rename(moved/"temporarily-missing-mask.png",maskPath);
    std::cout<<"Recipe round trip, manual baseline, range/version/hash/missing/ID failures, immutable source, locked overwrite, Chinese relocation and DDS passed\n";
    CoUninitialize();return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
