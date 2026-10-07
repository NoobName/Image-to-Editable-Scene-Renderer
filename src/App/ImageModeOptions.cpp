#include "App/ImageModeOptions.h"
#include "Assets/AssetIO.h"
#include "Core/Log.h"
#include <fstream>
#include <cmath>
namespace isr {
bool ImageModeOptions::Parse(const std::wstring& option,int& i,int argc,wchar_t** argv){
    if(option==L"--fixed-size"){fixedSize=true;return true;}
    if(option==L"--no-specular"){noSpecular=true;return true;}
    if(option==L"--image-fog-relative"){fogRelative=true;return true;}
    if(option==L"--image-fog-density"){
        if(i+1>=argc)throw std::invalid_argument("Missing image fog density");const std::wstring value=argv[++i];size_t count=0;
        const float density=std::stof(value,&count);ImageFogParameters p;p.density=density;
        if(count!=value.size()||!p.Valid())throw std::invalid_argument("Image fog density must be finite in [0,1000]");fogDensity=density;return true;
    }
    if(option==L"--protection-mask"){if(i+1>=argc)throw std::invalid_argument("Missing protection PNG");protectionMask=argv[++i];return true;}
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
        if(value!=L"fog-off"&&value!=L"fog-zero"&&value!=L"fog-reset"&&value!=L"fog-drag"&&value!=L"cycle"&&value!=L"input"&&value!=L"transaction"&&value!=L"views"&&value!=L"lighting-target"&&value!=L"lighting-source"
            &&value!=L"profile-drag"&&value!=L"ratio-drag"&&value!=L"ratio-reset"&&value!=L"ratio-zero"&&value!=L"ratio-color"&&value!=L"ratio-luminance"
            &&value!=L"stability-baseline"&&value!=L"stability-opposite"&&value!=L"stability-preset"
            &&value!=L"specular-move"&&value!=L"specular-off"&&value!=L"specular-rough"&&value!=L"specular-reset"&&value!=L"specular-invalid"
            &&value!=L"shadow-move"&&value!=L"shadow-off"&&value!=L"shadow-zero"&&value!=L"shadow-confidence-zero"&&value!=L"shadow-reset"
            &&value!=L"shadow-direct-off"&&value!=L"shadow-pcf"&&value!=L"shadow-scene-edit"
            &&value!=L"workspace-side"&&value!=L"workspace-wipe"&&value!=L"workspace-zoom"&&value!=L"workspace-protect"&&value!=L"workspace-clear"&&value!=L"workspace-exposure"&&value!=L"workspace-overlay"&&value!=L"workspace-global")throw std::invalid_argument("Unknown image smoke scenario");
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
    if(noSpecular)session.relighting.specularEnabled=false;
    waitForSource_=!session.SetMode(initial);
    if(waitForSource_)Log("Image mode unavailable: no validated anchor; 3D retained until a source is loaded");
}
void ImageModeOptions::Tick(unsigned frame,Scene& scene,Renderer& renderer,InputState& input){
    auto& session=renderer.Session();
    if(smoke=="fog-off")session.fog.enabled=false;
    if(smoke=="fog-zero")session.fog.density=0;
    if(smoke=="fog-reset"&&frame>=60)session.fog={};
    if(smoke=="fog-drag")session.fog.density=.15f+.1f*std::sin(float(frame)*.1f);
    if(smoke.rfind("workspace-",0)==0&&session.CanDisplayImage()){
        if(frame==10){session.lighting.target=session.lighting.source;session.lighting.target.direction[0]=-session.lighting.source.direction[0];
            session.relighting.shadowStrength=1;
            if(smoke=="workspace-side")session.display.comparison=1;
            if(smoke=="workspace-wipe"){session.display.comparison=2;session.display.wipe=.45f;}
            if(smoke=="workspace-zoom"){session.display.zoom=2;session.display.panX=.1f;session.display.panY=-.08f;}
            if(smoke=="workspace-protect"||smoke=="workspace-clear"){
                if(auto label=SourceLabel(*session.Source(),.1f,.5f)){session.protection.selected=label;session.protection.Apply(*label,1);}}
            if(smoke=="workspace-exposure")session.display.exposure=1;
            if(smoke=="workspace-global")session.lighting.targetGlobalGain=2;
            if(smoke=="workspace-overlay"){session.display.lowConfidence=true;session.display.confidenceThreshold=.9f;}}
        if(frame==50&&smoke=="workspace-clear")session.protection.Clear();
    }
    if(smoke.rfind("shadow-",0)==0&&session.lighting.available){
        session.relighting.shadowStrength=1;
        if(frame>=10){session.lighting.target=session.lighting.source;session.lighting.target.direction[0]=-session.lighting.source.direction[0];}
        if(smoke=="shadow-off")session.relighting.castShadows=false;
        if(smoke=="shadow-zero")session.relighting.shadowStrength=0;
        if(smoke=="shadow-confidence-zero")session.relighting.shadowConfidence=0;
        if(smoke=="shadow-direct-off")session.lighting.target.directIntensity=0;
        if(smoke=="shadow-pcf"){session.relighting.shadowPcfRadius=2;session.relighting.shadowBias=.001f;session.relighting.shadowNormalBias=.01f;}
        if(smoke=="shadow-reset"&&frame>=60)session.lighting.target=session.lighting.source;
        if(smoke=="shadow-scene-edit"&&frame==20){scene.camera.Move(2,1,1);for(auto& entity:scene.entities){entity.transform.position.x+=3;if(entity.renderer)entity.renderer->visible=false;}}
    }
    if(smoke.rfind("specular-",0)==0&&session.lighting.available){
        session.relighting.specularStrength=1;
        if(frame>=10){session.lighting.target=session.lighting.source;session.lighting.target.direction={-.35f,.1f,1};}
        if(smoke=="specular-off")session.relighting.specularEnabled=false;
        if(smoke=="specular-rough")session.relighting.specularRoughnessScale=1.7f;
        if(smoke=="specular-reset"&&frame>=60){session.lighting.target=session.lighting.source;session.relighting.specularRoughnessScale=1;}
        if(smoke=="specular-invalid")session.lighting.target.direction={0,0,0};
    }
    if(noSpecular)session.relighting.specularEnabled=false;
    if(smoke.rfind("stability-",0)==0&&session.lighting.available){
        if(frame==10){auto& light=session.lighting;light.target=light.source;light.target.directIntensity*=1.6f;
            if(smoke=="stability-baseline")session.relighting.stability=false;
            if(smoke=="stability-opposite")for(float& v:light.target.direction)v=-v;
            if(smoke=="stability-preset")session.relighting.Conservative();}
    }
    if((smoke.rfind("ratio-",0)==0||smoke=="profile-drag")&&session.lighting.available){
        auto& light=session.lighting;
        if(frame>=10&&(frame<60||smoke=="profile-drag")){light.target.direction={.4f*std::sin(float(frame)*.1f),.2f,1};light.target.directIntensity=light.source.directIntensity*1.4f;}
        if(smoke=="ratio-reset"&&frame>=60)light.target=light.source;
        if(smoke=="ratio-zero")session.relighting.strength=0;
        if(smoke=="ratio-color"||smoke=="ratio-luminance"){light.target.directColor={1,.2f,.05f};
            session.relighting.colorMode=smoke=="ratio-color"?RatioColorMode::BoundedColor:RatioColorMode::Luminance;}
    }
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
        session.imageView=frame<unsigned(ImageDebugCount*2)?static_cast<ImageDebugView>((frame/2)%ImageDebugCount):view;
        if(frame%2==0&&frame<unsigned(ImageDebugCount*2))Log(std::string("Image debug switch: ")+ImageDebugKeys[int(session.imageView)]);
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
        if(broken->shadow){auto shadow=std::make_shared<ShadowData>(*broken->shadow);shadow->maps[0].bytes.clear();broken->shadow=shadow;
            Log("Image transaction: injected malformed shadow upload candidate");
        }else if(broken->intrinsic){auto intrinsic=std::make_shared<IntrinsicData>(*broken->intrinsic);intrinsic->maps[0]->bytes.clear();broken->intrinsic=intrinsic;
            Log("Image transaction: injected malformed intrinsic upload candidate");
        }else if(broken->lighting){auto lighting=std::make_shared<LightingData>(*broken->lighting);lighting->maps[0].bytes.clear();broken->lighting=lighting;
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
        data["imageDisplay"]={{"zoom",session.display.zoom},{"pan",{session.display.panX,session.display.panY}},{"comparison",session.display.comparison},
            {"wipe",session.display.wipe},{"exposure",session.display.exposure},{"lowConfidence",session.display.lowConfidence}};
        data["regionProtection"]={{"revision",session.protection.revision},{"count",session.protection.weights.size()}};
        data["targetGlobalGain"]=session.lighting.targetGlobalGain;
        const auto& source=*session.Source();data["packageRoot"]=PathUtf8(source.packageRoot);
        if(int(session.imageView)>=2&&int(session.imageView)<15)data["analysisView"]=source.analysisMaps?source.analysisMaps->maps[size_t(session.imageView)-2].metadata:package::Json{{"status","unavailable"},{"reason","No analysis/analysis.json runtime maps"}};
        if(source.lighting){data["lighting"]=source.lighting->metadata;data["lighting"]["cacheValid"]=session.lighting.cacheValid;data["lighting"]["sourceRevision"]=session.lighting.sourceRevision;}
        if(source.intrinsic)data["intrinsic"]=source.intrinsic->metadata;
        if(source.shadow){data["shadow"]=source.shadow->metadata;data["shadow"]["cacheValid"]=session.lighting.cacheValid;}
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
