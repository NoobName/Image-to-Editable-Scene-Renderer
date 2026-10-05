#include "App/ImageModeOptions.h"
#include "Assets/AssetIO.h"
#include "Core/Log.h"
#include <fstream>
namespace isr {
bool ImageModeOptions::Parse(const std::wstring& option,int& i,int argc,wchar_t** argv){
    if(option==L"--fixed-size"){fixedSize=true;return true;}
    if(option!=L"--work-mode"&&option!=L"--image-view"&&option!=L"--window-size"&&option!=L"--image-smoke")return false;
    if(i+1>=argc)throw std::invalid_argument("Missing image-mode option value");const std::wstring value=argv[++i];
    if(option==L"--work-mode"){
        if(value!=L"scene"&&value!=L"image")throw std::invalid_argument("Work mode must be scene or image");
        initial=value==L"image"?WorkMode::ImageRelighting:WorkMode::Scene3D;
    }else if(option==L"--image-view"){
        const auto key=PathUtf8(std::filesystem::path(value));bool found=false;
        for(int v=0;v<ImageDebugCount;++v)if(key==ImageDebugKeys[v]){view=static_cast<ImageDebugView>(v);found=true;}
        if(!found)throw std::invalid_argument("Unknown image debug view");
    }else if(option==L"--image-smoke"){
        if(value!=L"cycle"&&value!=L"input"&&value!=L"transaction"&&value!=L"views"&&value!=L"lighting-target"&&value!=L"lighting-source")throw std::invalid_argument("Unknown image smoke scenario");
        smoke=PathUtf8(std::filesystem::path(value));
    }else{
        const auto split=value.find(L'x');size_t a=0,b=0;
        if(split==std::wstring::npos)throw std::invalid_argument("Window size must be WIDTHxHEIGHT");
        width=std::stoul(value.substr(0,split),&a);height=std::stoul(value.substr(split+1),&b);
        if(a!=split||b!=value.size()-split-1||!width||!height||width>4096||height>4096)throw std::invalid_argument("Window dimensions must be 1..4096");
    }
    return true;
}
void ImageModeOptions::Start(Renderer& renderer){
    auto& session=renderer.Session();session.imageView=view;
    waitForSource_=!session.SetMode(initial);
    if(waitForSource_)Log("Image mode unavailable: no validated anchor; 3D retained until a source is loaded");
}
void ImageModeOptions::Tick(unsigned frame,Scene& scene,Renderer& renderer,InputState& input){
    auto& session=renderer.Session();
    if((smoke=="lighting-target"||smoke=="lighting-source")&&frame==10){
        auto& light=session.lighting;if(!light.available)throw std::runtime_error("Lighting smoke requires lighting data");
        const auto source=light.source;light.target.direction={1,0,0};light.target.directIntensity=7;
        if(light.source!=source||!light.cacheValid)throw std::runtime_error("Target modified source/cache");
        if(smoke=="lighting-source"){light.draft=light.source;light.draft.directIntensity=2;
            if(!light.ApplySource()||light.cacheValid||light.target.directIntensity!=7)throw std::runtime_error("Source calibration/cache isolation failed");}
        Log("Lighting smoke: target/source isolated; cacheValid="+std::to_string(light.cacheValid));
    }
    if(waitForSource_&&session.CanDisplayImage()){session.SetMode(WorkMode::ImageRelighting);waitForSource_=false;}
    if(smoke=="transaction"&&frame>=5)TransactionSmoke(scene,renderer);
    if(smoke=="views"&&session.CanDisplayImage()){
        session.imageView=frame<70?static_cast<ImageDebugView>((frame/3)%ImageDebugCount):view;
        if(frame%3==0&&frame<70)Log(std::string("Image debug switch: ")+ImageDebugKeys[int(session.imageView)]);
    }
    if(smoke=="input"||smoke=="cycle"){
        input={};input.keys['W']=true;input.keys['R']=true;input.rightMouse=true;input.mouseX=8;input.mouseY=3;input.wheel=1;
    }
    if(smoke=="cycle"&&session.CanDisplayImage()){
        // Explicit automation only. Observe that independent source pixels survive all 3D mutations.
        if(frame==10){scene.camera.Move(2,1,1);scene.camera.Rotate(.4f,.2f);
            for(auto& e:scene.entities){e.transform.position.x+=3;if(e.renderer)e.renderer->visible=false;}
            for(auto& m:scene.materials){m.baseColor={1,0,0,1};m.metallic=1;}}
        if(frame%5==0&&frame<70){session.SetMode(frame%10==0?WorkMode::ImageRelighting:WorkMode::Scene3D);
            Log("Image mode cycle at frame="+std::to_string(frame));}
        if(frame>=70)session.SetMode(WorkMode::ImageRelighting);
        // In the cycle test only the explicit move above edits the free camera.
        if(session.Mode()==WorkMode::Scene3D)input={};
    }
}
void ImageModeOptions::TransactionSmoke(Scene& scene,Renderer& renderer){
    auto& session=renderer.Session();
    if(transactionStage_==0){
        if(!session.CanDisplayImage())throw std::runtime_error("Transaction smoke needs a source package");
        previousSource_=session.Source();previousRevision_=session.Revision();
        auto package=std::make_shared<ScenePackage>();package->scene=scene;
        auto broken=std::make_shared<SourceObservation>(*previousSource_);
        if(broken->lighting){auto lighting=std::make_shared<LightingData>(*broken->lighting);lighting->maps[0].bytes.clear();broken->lighting=lighting;
            Log("Image transaction: injected malformed lighting upload candidate");
        }else if(broken->analysisMaps){
            // Test-only corruption after CPU validation: exercise the numeric GPU preparation rollback.
            auto maps=std::make_shared<AnalysisMaps>(*broken->analysisMaps);
            bool injected=false;for(auto& map:maps->maps)if(map.image){map.image->bytes.clear();injected=true;break;}
            if(!injected)throw std::runtime_error("Analysis transaction fixture needs an available numeric map");
            broken->analysisMaps=maps;Log("Image transaction: injected malformed numeric upload candidate");
        }else{
            auto image=std::make_shared<ImageData>(*broken->anchor->pixels);image->rgba.clear();broken->anchor->pixels=image;
        }
        package->observation=broken;renderer.PrepareScene(package);transactionStage_=1;
    }else if(transactionStage_==1&&renderer.ScenePrepared()){
        bool rejected=false;
        try{renderer.CommitPreparedScene();}catch(const std::exception& e){
            const char* expected=previousSource_->analysisMaps?"Numeric upload byte count mismatch":"Source image pixel size mismatch";
            if(std::string(e.what()).find(expected)==std::string::npos)throw;rejected=true;
        }
        if(!rejected||session.Source()!=previousSource_||session.Revision()!=previousRevision_)throw std::runtime_error("Failed preparation changed source session");
        Log("Image transaction: failed GPU preparation retained scene/source/revision");
        auto valid=std::make_shared<ScenePackage>();valid->scene=scene;valid->observation=previousSource_;
        renderer.PrepareScene(valid);transactionStage_=2;
    }else if(transactionStage_==2&&renderer.ScenePrepared()){
        renderer.CommitPreparedScene(true);
        if(session.Source()!=previousSource_||session.Revision()!=previousRevision_)throw std::runtime_error("Cancelled preparation changed source session");
        Log("Image transaction: cancelled completed upload retained scene/source/revision");transactionStage_=3;
    }
}
void ImageModeOptions::Report(const std::filesystem::path& capture,const Scene& scene,const Renderer& renderer,const RenderSettings& settings)const{
    if(capture.empty())return;
    auto reportPath=capture;reportPath.replace_extension(".view.json");
    const auto& session=renderer.Session();const auto p=scene.camera.Position();const auto angles=scene.camera.Angles();
    package::Json data={{"workMode",session.Mode()==WorkMode::Scene3D?"scene":"image"},{"renderMode",int(settings.mode)},
        {"imageView",ImageDebugKeys[int(session.imageView)]},{"revision",session.Revision()},
        {"freeCamera",{{"position",{p.x,p.y,p.z}},{"angles",{angles.x,angles.y}},{"aspect",scene.camera.Aspect()},{"fov",scene.camera.Fov()}}}};
    if(session.Source()){
        const auto& source=*session.Source();data["packageRoot"]=PathUtf8(source.packageRoot);
        if(int(session.imageView)>=2&&int(session.imageView)<15)data["analysisView"]=source.analysisMaps?source.analysisMaps->maps[size_t(session.imageView)-2].metadata:package::Json{{"status","unavailable"},{"reason","No analysis/analysis.json runtime maps"}};
        if(source.lighting){data["lighting"]=source.lighting->metadata;data["lighting"]["cacheValid"]=session.lighting.cacheValid;data["lighting"]["sourceRevision"]=session.lighting.sourceRevision;}
        if(source.anchor){const auto& a=*source.anchor;data["sourceId"]=a.metadata["sourceId"];data["sourceSha256"]=a.metadata["sourceImage"]["sha256"];
            data["sourceSize"]=a.sourceSize;data["analysisSize"]=a.analysisSize;data["sourceCamera"]=a.metadata["sourceCamera"];
            data["analysisMapping"]=a.metadata["analysisMapping"];data["capabilities"]=a.metadata["capabilities"];
            const auto w=renderer.ViewWidth(),h=renderer.ViewHeight();data["viewportSize"]={w,h};const auto rect=FitSourceImage(a.sourceSize[0],a.sourceSize[1],w,h);
            data["imageRect"]={rect.x,rect.y,rect.width,rect.height};data["displayScale"]=rect.scale;}
    }
    std::ofstream stream(reportPath,std::ios::binary);stream.exceptions(std::ios::badbit|std::ios::failbit);stream<<data.dump(2)<<'\n';
    Log("Saved source-view diagnostics: "+PathUtf8(reportPath));
}
}
