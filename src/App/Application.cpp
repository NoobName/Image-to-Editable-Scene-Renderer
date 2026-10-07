#include "App/Application.h"
#include "App/Window.h"
#include "Renderer/Renderer.h"
#include "Core/Log.h"
#include "App/CameraController.h"
#include "Assets/AssetManager.h"
#include "Assets/AssetIO.h"
#include "ScenePackage/ScenePackageLoader.h"
#include "App/LightingOptions.h"
#include "App/LookOptions.h"
#include "App/ReconstructionSession.h"
#include "App/EditorSmoke.h"
#include "App/ImageModeOptions.h"
#include "App/RecipeWorkflow.h"
#include "App/ReferenceWorkflow.h"
#include "App/RefinementWorkflow.h"
#include <chrono>
#include <string>
#include <algorithm>
#include <cmath>
#include <utility>
namespace isr {
int RunApplication(HINSTANCE instance, int argc, wchar_t** argv) {
    bool warp = false, smoke = false, reverseOrder = false, cameraSmoke = false;
    std::optional<bool> ui;bool materialSmoke=false;RenderSettings settings;std::wstring lightSelection=L"all";
    bool environmentSmoke=false,lightingTest=false,lookSmoke=false,explicitEnvironment=false;
    EditorSmoke editorSmoke;
    ImageModeOptions imageOptions;
    RecipeWorkflow recipeWorkflow;
    RefinementWorkflow refinementWorkflow;
    ReferenceWorkflow referenceWorkflow;
    unsigned referenceCancelFrame=0,referenceRepeat=1,optimizationEditFrame=0;ReferenceAction repeatReferenceAction=ReferenceAction::None;
    std::filesystem::path recipePath,saveRecipePath,exportImagePath,recipeReloadPath;
    std::filesystem::path profilePath;
    bool replaceRecipe=false,recipeCancel=false;
    bool refinementRun=false;unsigned refinementCancelFrame=0,refinementEditFrame=0;
    std::optional<RelightingRecipe> startupRecipe;
    unsigned frameLimit = 0; Demo demo = Demo::Scene; std::filesystem::path capture,model,packagePath;std::string objectSmoke;
    wchar_t executable[32768]{};
    if (!GetModuleFileNameW(nullptr, executable, 32768)) Check(HRESULT_FROM_WIN32(GetLastError()));
    ReconstructionSession reconstruction(std::filesystem::path(executable).parent_path());
    std::filesystem::path reconstructImage,nextReconstructImage;unsigned cancelReconstructionFrame=0,reconstructionRepeat=1;bool lightingOnlyJob=false,intrinsicOnlyJob=false,shadowOnlyJob=false;
    std::filesystem::path log = std::filesystem::path(executable).parent_path() / L"renderer.log";
    for (int i = 1; i < argc; ++i) {
        const std::wstring argument = argv[i];
        if(argument==L"--profile"&&i+1<argc){profilePath=argv[++i];continue;}
        if(imageOptions.Parse(argument,i,argc,argv))continue;
        if((argument==L"--reference-input"||argument==L"--reference-proposal"||argument==L"--optimize-reference")&&i+1<argc){
            if(referenceWorkflow.actions.request!=ReferenceAction::None)throw std::invalid_argument("Choose one reference input");
            referenceWorkflow.actions.input=argv[++i];referenceWorkflow.actions.request=argument==L"--reference-input"?ReferenceAction::Analyze:argument==L"--optimize-reference"?ReferenceAction::Optimize:ReferenceAction::Load;repeatReferenceAction=referenceWorkflow.actions.request;continue;}
        if(argument==L"--optimization-registered"){referenceWorkflow.actions.registered=true;continue;}
        if(argument==L"--optimization-iterations"&&i+1<argc){referenceWorkflow.actions.iterations=std::stoi(argv[++i]);if(referenceWorkflow.actions.iterations<1||referenceWorkflow.actions.iterations>200)throw std::invalid_argument("Optimization iterations must be 1..200");continue;}
        if(argument==L"--optimization-edit-frame"&&i+1<argc){optimizationEditFrame=std::stoul(argv[++i]);continue;}
        if(argument==L"--reference-relation"&&i+1<argc){referenceWorkflow.actions.relation=PathUtf8(argv[++i]);if(referenceWorkflow.actions.relation!="same-scene"&&referenceWorkflow.actions.relation!="different-content")throw std::invalid_argument("Unknown reference relation");continue;}
        if(argument==L"--apply-reference"){referenceWorkflow.autoApply=true;continue;}
        if(argument==L"--reset-reference"){referenceWorkflow.resetAfterApply=true;continue;}
        if(argument==L"--reference-dummy"){referenceWorkflow.actions.useConfiguredBackends=false;continue;}
        if(argument==L"--reference-cancel-frame"&&i+1<argc){referenceCancelFrame=std::stoul(argv[++i]);continue;}
        if(argument==L"--reference-repeat"&&i+1<argc){referenceRepeat=std::stoul(argv[++i]);if(referenceRepeat<1||referenceRepeat>5)throw std::invalid_argument("Reference repeat must be 1..5");continue;}
        if(argument==L"--recipe"&&i+1<argc){recipePath=argv[++i];continue;}
        if(argument==L"--save-recipe"&&i+1<argc){saveRecipePath=argv[++i];continue;}
        if(argument==L"--replace-recipe"){replaceRecipe=true;continue;}
        if(argument==L"--export-image"&&i+1<argc){exportImagePath=argv[++i];continue;}
        if(argument==L"--recipe-reload"&&i+1<argc){recipeReloadPath=argv[++i];continue;}
        if(argument==L"--recipe-cancel"){recipeCancel=true;continue;}
        if(argument==L"--refinement-run"){refinementRun=true;continue;}
        if(argument==L"--refinement-strength"&&i+1<argc){size_t used=0;const std::wstring value=argv[++i];recipeWorkflow.actions.refinementStrength=std::stof(value,&used);
            if(used!=value.size()||!std::isfinite(recipeWorkflow.actions.refinementStrength)||recipeWorkflow.actions.refinementStrength<0||recipeWorkflow.actions.refinementStrength>1)throw std::invalid_argument("Refinement strength must be finite in [0,1]");continue;}
        if(argument==L"--refinement-cancel-frame"&&i+1<argc){refinementCancelFrame=std::stoul(argv[++i]);continue;}
        if(argument==L"--refinement-edit-frame"&&i+1<argc){refinementEditFrame=std::stoul(argv[++i]);continue;}
        if(ParseLookOption(argument,i,argc,argv,settings.look))continue;
        if(ParseLightingOption(argument,i,argc,argv,settings,environmentSmoke,lightingTest)){if(argument==L"--env")explicitEnvironment=true;continue;}
        if (argument == L"--warp") warp = true;
        else if(argument==L"--reconstruct"&&i+1<argc)reconstructImage=argv[++i];
        else if(argument==L"--fit-lighting"&&i+1<argc){reconstructImage=argv[++i];lightingOnlyJob=true;}
        else if(argument==L"--estimate-shadows"&&i+1<argc){reconstructImage=argv[++i];shadowOnlyJob=true;}
        else if(argument==L"--estimate-intrinsic"&&i+1<argc){reconstructImage=argv[++i];intrinsicOnlyJob=true;}
        else if(argument==L"--intrinsic-backend"&&i+1<argc)reconstruction.manager.options.intrinsic=PathUtf8(std::filesystem::path(argv[++i]));
        else if(argument==L"--lighting-backend"&&i+1<argc)reconstruction.manager.options.lighting=PathUtf8(std::filesystem::path(argv[++i]));
        else if(argument==L"--reconstruction-next-image"&&i+1<argc)nextReconstructImage=argv[++i];
        else if(argument==L"--reconstruction-python"&&i+1<argc)reconstruction.manager.options.python=argv[++i];
        else if(argument==L"--reconstruction-cancel-frame"&&i+1<argc)cancelReconstructionFrame=std::stoul(argv[++i]);
        else if(argument==L"--reconstruction-repeat"&&i+1<argc){reconstructionRepeat=std::stoul(argv[++i]);if(!reconstructionRepeat||reconstructionRepeat>10)throw std::invalid_argument("Reconstruction repeat must be 1..10");}
        else if(argument==L"--reconstruction-preset"&&i+1<argc){
            const std::wstring preset=argv[++i];if(preset!=L"dummy"&&preset!=L"full")throw std::invalid_argument("Reconstruction preset must be dummy or full");
            auto& options=reconstruction.manager.options;const bool dummy=preset==L"dummy";
            options.geometry=dummy?"dummy":"moge";options.segmentation=dummy?"dummy":"sam2";options.materials=dummy?"neutral":"marigold";
        }
        else if(argument==L"--ui")ui=true;
        else if(argument==L"--no-ui")ui=false;
        else if(argument==L"--material-smoke")materialSmoke=true;
        else if(argument==L"--object-smoke"&&i+1<argc)objectSmoke=PathUtf8(std::filesystem::path(argv[++i]));
        else if(argument==L"--editor-smoke"&&i+1<argc)editorSmoke.mode=PathUtf8(std::filesystem::path(argv[++i]));
        else if(argument==L"--editor-object"&&i+1<argc)editorSmoke.objectId=PathUtf8(std::filesystem::path(argv[++i]));
        else if(argument==L"--look-smoke")lookSmoke=true;
        else if(argument==L"--render-mode"&&i+1<argc){
            const std::wstring value=argv[++i];const wchar_t* names[]={L"final",L"albedo",L"normal",L"roughness",L"metallic",L"depth",L"wireframe",
                L"original",L"estimated-albedo",L"estimated-normal",L"estimated-roughness"};
            static_assert(sizeof(names)/sizeof(names[0])==RenderModeCount);
            int index=0;for(;index<RenderModeCount&&value!=names[index];++index){}if(index==RenderModeCount)throw std::invalid_argument("Unknown render mode");settings.mode=static_cast<RenderMode>(index);
        }
        else if(argument==L"--ambient"&&i+1<argc){settings.ambient=std::stof(argv[++i]);if(!std::isfinite(settings.ambient)||settings.ambient<0||settings.ambient>1)throw std::invalid_argument("Ambient must be between 0 and 1");}
        else if(argument==L"--lights"&&i+1<argc)lightSelection=argv[++i];
        else if (argument == L"--demo" && i + 1 < argc) {
            const std::wstring mode = argv[++i];
            if (mode == L"clear") demo = Demo::Clear;
            else if (mode == L"triangle") demo = Demo::Triangle;
            else if (mode == L"scene") demo = Demo::Scene;
            else throw std::runtime_error("Unknown demo");
        }
        else if (argument == L"--smoke") smoke = true;
        else if (argument == L"--reverse-order") reverseOrder = true;
        else if (argument == L"--camera-smoke") cameraSmoke = true;
        else if (argument == L"--capture" && i + 1 < argc) capture = argv[++i];
        else if (argument == L"--model" && i + 1 < argc) model = argv[++i];
        else if (argument == L"--package" && i + 1 < argc) packagePath = argv[++i];
        else if (argument == L"--frames" && i + 1 < argc) frameLimit = std::stoul(argv[++i]);
        else if (argument == L"--log" && i + 1 < argc) log = argv[++i];
        else throw std::runtime_error("Unknown or incomplete command-line argument");
    }
    OpenLog(log);
    if(!recipePath.empty()){
        if(!packagePath.empty()||!model.empty()||!imageOptions.protectionMask.empty())throw std::invalid_argument("--recipe owns its package and protection mask");
        startupRecipe=LoadRecipe(recipePath);packagePath=startupRecipe->package->root;
    }
    if((!saveRecipePath.empty()||!exportImagePath.empty()||!recipeReloadPath.empty())&&!frameLimit)throw std::invalid_argument("Recipe CLI save/export/reload requires --frames");
    if (!capture.empty() && !frameLimit) throw std::invalid_argument("--capture requires --frames");
    if(!model.empty()&&!packagePath.empty()) throw std::invalid_argument("--model and --package are mutually exclusive");
    if(!packagePath.empty()&&demo!=Demo::Scene) throw std::invalid_argument("--package requires --demo scene");
    if(!reconstructImage.empty()&&demo!=Demo::Scene)throw std::invalid_argument("--reconstruct requires --demo scene");
    if(!editorSmoke.mode.empty()&&(!reconstructImage.empty()||!frameLimit||demo!=Demo::Scene))throw std::invalid_argument("Editor smoke requires a finite scene run without reconstruction");
    if(!imageOptions.smoke.empty()&&(!reconstructImage.empty()||!frameLimit||demo!=Demo::Scene))throw std::invalid_argument("Image smoke requires a finite scene run without reconstruction");
    const auto cpuPreparationStart=std::chrono::steady_clock::now();
    AssetManager assets;Scene scene;CameraController controller;std::shared_ptr<const SourceObservation> observation;
    if(packagePath.empty()) scene=model.empty()?Scene::CreateDemo():assets.LoadModel(model);
    else {
        auto package=startupRecipe?std::move(*startupRecipe->package):ScenePackageLoader{}.Load(packagePath);scene=std::move(package.scene);
        observation=std::move(package.observation);
        settings.look=package.look;settings.environmentPath=package.environment.hdri;
        settings.environmentIntensity=package.environment.intensity;settings.environmentRotation=package.environment.rotation;
        settings.ibl=package.environment.ibl;settings.skybox=package.environment.skybox;
        // An explicit replacement HDRI activates lighting unless --no-ibl/--no-sky are supplied.
        if(explicitEnvironment) {settings.ibl=true;settings.skybox=true;}
        // Package supplies the baseline. Explicit CLI overrides win regardless of argument order.
        for(int i=1;i<argc;++i) {
            const std::wstring argument=argv[i];
            if(ParseLookOption(argument,i,argc,argv,settings.look)) continue;
            if(ParseLightingOption(argument,i,argc,argv,settings,environmentSmoke,lightingTest)) continue;
            // Skip operands of other switches, even when a filename happens to start with --.
            if(argument==L"--image-fog-density"||argument==L"--profile"||argument==L"--package"||argument==L"--model"||argument==L"--log"||argument==L"--capture"||argument==L"--frames"||
                argument==L"--demo"||argument==L"--render-mode"||argument==L"--ambient"||argument==L"--lights"||argument==L"--object-smoke"||argument==L"--editor-smoke"||argument==L"--editor-object"||
                argument==L"--reconstruct"||argument==L"--fit-lighting"||argument==L"--estimate-intrinsic"||argument==L"--estimate-shadows"||argument==L"--intrinsic-backend"||argument==L"--lighting-backend"||argument==L"--reconstruction-python"||argument==L"--reconstruction-preset"||argument==L"--reconstruction-cancel-frame"||argument==L"--reconstruction-repeat"||
                argument==L"--work-mode"||argument==L"--image-view"||argument==L"--window-size"||argument==L"--image-smoke"||argument==L"--protection-mask"||argument==L"--reconstruction-next-image"||
                argument==L"--recipe"||argument==L"--save-recipe"||argument==L"--export-image"||argument==L"--recipe-reload"||
                argument==L"--reference-input"||argument==L"--reference-proposal"||argument==L"--reference-relation"||argument==L"--reference-cancel-frame"||argument==L"--reference-repeat"||argument==L"--optimize-reference"||argument==L"--optimization-iterations"||argument==L"--optimization-edit-frame") ++i;
        }
        Log("Package look: exposure="+std::to_string(settings.look.exposure)+"; environment intensity="+std::to_string(settings.environmentIntensity));
    }
    if(lightingTest){if(!model.empty()||!packagePath.empty())throw std::invalid_argument("--lighting-test uses the procedural scene");
        scene.materials[1].baseColor={0.9f,0.9f,0.9f,1};scene.materials[1].metallic=1;scene.materials[1].roughness=0.12f;
        scene.materials[0].baseColor={0.6f,0.6f,0.6f,1};scene.materials[2].baseColor={0.24f,0.24f,0.24f,1};}
    if(lightSelection!=L"all"&&lightSelection!=L"directional"&&lightSelection!=L"point"&&lightSelection!=L"none")throw std::invalid_argument("Unknown lights selection");
    if(lightSelection!=L"all")std::erase_if(scene.lights,[&](const Light& light){return lightSelection==L"none"||(lightSelection==L"point"?light.type!=LightType::Point:light.type!=LightType::Directional);});
    std::optional<size_t> objectSmokeRoot;
    if(!objectSmoke.empty()) {
        const auto found=std::find_if(scene.entities.begin(),scene.entities.end(),[&](const Entity& e){return e.objectId==objectSmoke;});
        if(found==scene.entities.end())throw std::invalid_argument("Object smoke ID was not found");
        objectSmokeRoot=size_t(found-scene.entities.begin());
    }
    Log("Startup CPU scene/package preparation ms="+std::to_string(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-cpuPreparationStart).count())+" (file validation/decode; excludes AI)");
    const auto gpuPreparationStart=std::chrono::steady_clock::now();
    Window window(instance,imageOptions.width,imageOptions.height);
    editorSmoke.Initialize(scene);
    Renderer renderer(window.Handle(), window.Width(), window.Height(), warp, demo, scene,ui.value_or(frameLimit==0),settings.environmentPath,std::move(observation),startupRecipe?startupRecipe->importedMask:LoadProtectionMask(imageOptions.protectionMask));
    Log("Startup GPU/window preparation ms="+std::to_string(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-gpuPreparationStart).count())+" (includes uploads, shader compile, environment; excludes AI)");
    imageOptions.Start(renderer);
    if(startupRecipe)renderer.Session().RestoreState(std::move(startupRecipe->session));
    imageOptions.ApplyFog(renderer);
    renderer.BindRecipe(window.Handle(),&recipeWorkflow.actions);
    renderer.BindReference(window.Handle(),&referenceWorkflow.actions);
    if(repeatReferenceAction!=ReferenceAction::None&&!renderer.Session().SetMode(WorkMode::ImageRelighting))throw std::runtime_error("Reference matching requires source image mode");
    if(auto selected=editorSmoke.Selection())renderer.SelectEntity(*selected);
    renderer.BindReconstruction(window.Handle(),&reconstruction.manager);
    if(!reconstructImage.empty())reconstruction.manager.Start(reconstructImage,lightingOnlyJob,intrinsicOnlyJob,shadowOnlyJob);
    window.SetMessageHandler([&](HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){return renderer.HandleMessage(hwnd,msg,wp,lp);});
    struct MessageHandlerLifetime {
        Window& window;
        ~MessageHandlerLifetime(){window.SetMessageHandler({});}
    } messageHandlerLifetime{window}; // Also detach before Renderer destruction during exception unwinding.
    Log("Window created; demo=" + std::to_string(static_cast<int>(demo)));
    if(refinementRun)Log("Refinement finite-frame limit="+std::to_string(frameLimit));
    unsigned frames = 0;
    unsigned reconstructionEndFrame=0;const auto reconstructionStart=std::chrono::steady_clock::now();
    auto previousReconstructionState=ReconstructionState::Idle;
    auto previous = std::chrono::steady_clock::now();
    if(!profilePath.empty())renderer.EnableProfiling();
    while (window.Pump()) {
        const auto now = std::chrono::steady_clock::now();
        const float dt = std::chrono::duration<float>(now-previous).count(); previous = now;
        reconstruction.Tick(renderer,scene,settings,controller);
        if(frames==10&&!recipeReloadPath.empty()){recipeWorkflow.actions.path=recipeReloadPath;recipeWorkflow.actions.request=RecipeAction::Open;}
        if(recipeCancel&&frames==11)recipeWorkflow.actions.cancel=true;
        if(refinementRun&&frames==10){recipeWorkflow.actions.request=RecipeAction::Refine;recipeWorkflow.actions.focusRefinement=true;}
        if(refinementCancelFrame&&frames==refinementCancelFrame)recipeWorkflow.actions.cancelRefinement=true;
        if(refinementEditFrame&&frames==refinementEditFrame)renderer.Session().display.exposure=.25f;
        refinementWorkflow.Tick(renderer,recipeWorkflow.actions,reconstruction.manager.options,reconstruction.manager.Status().Busy());
        recipeWorkflow.Tick(renderer,scene,settings,controller,reconstruction.manager.Status().Busy());
        if(referenceCancelFrame&&frames==referenceCancelFrame)referenceWorkflow.actions.cancel=true;
        if(optimizationEditFrame&&frames==optimizationEditFrame){renderer.Session().lighting.target.directIntensity=.123f;Log("Optimization concurrency test: user target changed to 0.123");}
        const bool referenceWasBusy=referenceWorkflow.actions.busy;
        referenceWorkflow.Tick(renderer,reconstruction.manager.options,recipeWorkflow.actions.busy||reconstruction.manager.Status().Busy());
        if(referenceWasBusy&&!referenceWorkflow.actions.busy&&referenceRepeat>1&&!referenceWorkflow.actions.cancel){--referenceRepeat;referenceWorkflow.actions.request=repeatReferenceAction;}
        const auto reconstructionState=reconstruction.manager.Status().state;
        if(reconstructionState!=previousReconstructionState){Log(std::string("Reconstruction state ")+ReconstructionStateName(reconstructionState)+" at rendered frame="+std::to_string(frames));previousReconstructionState=reconstructionState;}
        if(!reconstructImage.empty()&&!reconstruction.manager.Status().Busy()&&!reconstructionEndFrame){
            reconstructionEndFrame=frames+frameLimit;Log("Reconstruction terminal after rendered frames="+std::to_string(frames));objectSmokeRoot.reset();}
        if(cancelReconstructionFrame&&frames==cancelReconstructionFrame)reconstruction.manager.Cancel();
        if(!reconstructImage.empty()&&frameLimit&&now-reconstructionStart>std::chrono::minutes(20))throw std::runtime_error("Reconstruction smoke timed out");
        if (window.Minimized() || !window.Width() || !window.Height()) { window.ConsumeInput();
            MsgWaitForMultipleObjectsEx(0,nullptr,100,QS_ALLINPUT,MWMO_INPUTAVAILABLE);previous = std::chrono::steady_clock::now();continue; }
        auto input = window.ConsumeInput();
        renderer.Resize(window.Width(), window.Height());
        renderer.UpdateUI(scene,settings,input);
        editorSmoke.Tick(frames,scene,settings);
        if(objectSmokeRoot&&(frames==30||frames==50||frames==70)) {
            const auto targets=scene.RenderableSubtree(*objectSmokeRoot);
            for(auto index:targets) {
                auto& rendererData=*scene.entities[index].renderer;
                if(frames<70)rendererData.visible=frames==50;
                else {auto& material=scene.materials.at(rendererData.materialIndex);material.baseColor={0.15f,0.8f,0.25f,1};material.roughness=0.15f;}
            }
            Log("Object smoke: "+objectSmoke+" frame="+std::to_string(frames)+" primitives="+std::to_string(targets.size()));
        }
        if(lookSmoke&&(frames==30||frames==70)){
            auto& look=settings.look;look.exposure=0.4f;look.temperature=0.6f;look.tint=0.3f;look.saturation=1.2f;look.contrast=1.2f;
            look.bloomEnabled=true;look.bloomIntensity=0.4f;look.vignetteIntensity=0.55f;
            Log("Look smoke: shared parameters changed at frame "+std::to_string(frames));
        }
        if(lookSmoke&&frames==50){settings.look=LookParameters{};Log("Look smoke: reset at frame 50");}
        if(environmentSmoke&&frames==30)settings.environmentPath=EnvironmentManager::DefaultPath().parent_path()/"SunsetCourtyard.hdr";
        if(environmentSmoke&&frames==50)settings.environmentPath=EnvironmentManager::DefaultPath().parent_path()/"missing-validation-environment.hdr";
        if(environmentSmoke&&frames==70)settings.environmentPath=EnvironmentManager::DefaultPath();
        if(materialSmoke&&frames==30&&!scene.materials.empty()){
            auto& m=scene.materials[0];m.baseColor={0.15f,0.8f,0.25f,1};m.metallic=0.3f;m.roughness=0.7f;m.ao=0.25f;m.emissive={6,1,0.25f};
            Log("Material smoke: live base/metal/rough/AO/emissive changed at frame 30");
        }
        if (cameraSmoke) {
            // Deterministic controller exercise, independent of desktop focus and frame time.
            input = {}; input.active = true;
            if (frames < 20) input.keys['W'] = true;
            else if (frames < 40) { input.rightMouse = true; input.mouseX = 2; input.mouseY = 0.5f; }
            else if (frames < 60) { input.rightMouse = true; input.keys[VK_MENU] = true; input.mouseX = 2; }
            else if (frames == 60) input.wheel = 1;
        }
        imageOptions.Tick(frames,scene,renderer,input);
        controller.Update(scene.camera,input,cameraSmoke ? 1.0f/60.0f : dt,renderer.Session().Mode());
        if(renderer.Session().Mode()==WorkMode::Scene3D)scene.camera.SetAspect(renderer.SceneAspect()); // Source projection is never resized.
        scene.UpdateWorldMatrices();
        auto endFrame=reconstructImage.empty()?frameLimit:reconstructionEndFrame;
        if(recipeWorkflow.actions.refinementBusy){endFrame=0;if(frameLimit&&now-reconstructionStart>std::chrono::minutes(5))throw std::runtime_error("Refinement smoke timed out");}
        else if(refinementRun&&frameLimit)endFrame=std::max(frameLimit,frames+1);
        if(imageOptions.Busy())endFrame=0;
        else if(imageOptions.smoke=="transaction"&&frameLimit)endFrame=std::max(frameLimit,frames+1);
        // A finite recipe load captures the committed document, not a frame from the old
        // scene while its candidate is still uploading. The native export uses the same state.
        if(recipeWorkflow.actions.busy){endFrame=0;if(frameLimit&&now-reconstructionStart>std::chrono::seconds(60))throw std::runtime_error("Recipe smoke timed out");}
        else if(!recipeReloadPath.empty()&&frameLimit)endFrame=std::max(frameLimit,frames+1);
        if(referenceWorkflow.actions.busy||referenceWorkflow.actions.request!=ReferenceAction::None){endFrame=0;if(frameLimit&&now-reconstructionStart>std::chrono::minutes(20))throw std::runtime_error("Reference smoke timed out");}
        else if(repeatReferenceAction!=ReferenceAction::None&&frameLimit)endFrame=std::max(frameLimit,frames+1);
        // Completion of one independent task must not override another task's pending fence/load.
        if(recipeWorkflow.actions.refinementBusy||recipeWorkflow.actions.busy||referenceWorkflow.actions.busy||referenceWorkflow.actions.request!=ReferenceAction::None||imageOptions.Busy()||reconstruction.manager.Status().Busy())endFrame=0;
        const bool capturing=frameLimit&&endFrame&&frames+1==endFrame&&!capture.empty();
        renderer.Render(scene,settings,reverseOrder,capturing?capture:std::filesystem::path{}); ++frames;
        renderer.ProfileCpuFrame(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-now).count(),capturing);
        if (smoke && !imageOptions.fixedSize && frames == 20) window.SetClientSize(960, 540);
        if (smoke && !imageOptions.fixedSize && frames == 40) window.SetClientSize(1280, 720);
        if (smoke && frames == 60) window.TestMinimizeRestore();
        if (frameLimit && endFrame && frames >= endFrame){
            if(!reconstructImage.empty()&&reconstructionRepeat>1&&reconstruction.manager.Status().state==ReconstructionState::Ready){
                --reconstructionRepeat;reconstructionEndFrame=0;
                if(!nextReconstructImage.empty())reconstructImage=std::exchange(nextReconstructImage,{});
                reconstruction.manager.Start(reconstructImage,lightingOnlyJob,intrinsicOnlyJob,shadowOnlyJob);
            }else break;
        }
    }
    renderer.Finish();
    if(!profilePath.empty())renderer.SaveProfile(profilePath);
    Log("Recipe workflow: "+recipeWorkflow.actions.status);
    Log("Refinement workflow: "+recipeWorkflow.actions.refinementStatus);
    Log("Reference workflow: "+referenceWorkflow.actions.status);
    if(!saveRecipePath.empty())SaveRecipe(saveRecipePath,renderer.Session(),renderer.ImportedProtection(),replaceRecipe);
    if(!exportImagePath.empty())renderer.ExportImage(exportImagePath);
    imageOptions.Report(capture,scene,renderer,settings);
    window.SetMessageHandler({});
    const auto position = scene.camera.Position();
    Log("Camera position: " + std::to_string(position.x) + ", " + std::to_string(position.y) + ", " + std::to_string(position.z));
    Log("Completed frames=" + std::to_string(frames));
    return !reconstructImage.empty()&&reconstruction.manager.Status().state==ReconstructionState::Error?2:0;
}
}
