#include "UI/ImageWorkspace.h"
#include "UI/ImagePointLights.h"
#include "ScenePackage/RelightingRecipe.h"
#include <iostream>
using namespace isr;
namespace {void Require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}}
int main(){try{
    RelightingSession s;auto source=std::make_shared<SourceObservation>();source->anchor.emplace();
    source->anchor->sourceSize={500,300};source->anchor->analysisSize={4,2};source->anchor->pixels=std::make_shared<ImageData>();
    auto maps=std::make_shared<AnalysisMaps>();NumericImage labels;labels.width=4;labels.height=2;labels.format=NumericFormat::Label;
    const uint32_t ids[]{7,7,42,42,7,7,42,42};labels.bytes.resize(sizeof(ids));std::memcpy(labels.bytes.data(),ids,sizeof(ids));maps->maps[5].image=labels;source->analysisMaps=maps;
    maps->metadata={{"camera",{{"intrinsicsNormalized",{1.,0.,.5,0.,1.,.5,0.,0.,1.}}}}};
    for(size_t key:{1,3,4}){NumericImage im;im.width=4;im.height=2;im.format=key==4?NumericFormat::Label:NumericFormat::Vector;im.bytes.resize(8*im.Channels()*4);
        for(size_t pixel=0;pixel<8;++pixel){if(key==4){uint32_t valid=1;std::memcpy(im.bytes.data()+pixel*4,&valid,4);}else{float p[4]{0,0,key==1?-1.f:2.f,0};std::memcpy(im.bytes.data()+pixel*16,p,16);}}maps->maps[key].image=im;}
    source->lighting=std::make_shared<LightingData>();
    source->regions={{7,"stable-left","Left"},{42,"stable-right","Right"}};s.Publish(source);s.SetMode(WorkMode::ImageRelighting);
    s.lighting.available=true;s.lighting.source.direction={.3f,0,std::sqrt(.91f)};s.lighting.target=s.lighting.source;
    const auto original=s.lighting.source;const auto sourceBytes=labels.bytes;
    Require(SourceLabel(*source,.1f,.5f)==7&&SourceLabel(*source,.9f,.5f)==42&&!SourceLabel(*source,-.1f,0),"Source nearest labels / bounds wrong");
    ImGui::CreateContext();struct Cleanup{~Cleanup(){ImGui::DestroyContext();}}cleanup;
    auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize={800,620};io.DeltaTime=1.f/60;io.ConfigInputTrickleEventQueue=false;io.ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard;
    unsigned char* pixels;int fw,fh;io.Fonts->GetTexDataAsRGBA32(&pixels,&fw,&fh);
    ImVec2 origin{},textMin{},textMax{},sliderMin{},sliderMax{},applyMin{},applyMax{},pointMin{},pointMax{};
    char text[1024]="D:/test/long path";bool popup=false;float slider=.4f;
    auto frame=[&]{ImGui::NewFrame();ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize({780,600});ImGui::Begin("Workspace");
        ImGui::InputText("Path",text,sizeof(text));textMin=ImGui::GetItemRectMin();textMax=ImGui::GetItemRectMax();
        ImGui::SliderFloat("Focus slider",&slider,0,1);sliderMin=ImGui::GetItemRectMin();sliderMax=ImGui::GetItemRectMax();
        DrawApplyRegionProtection(s);applyMin=ImGui::GetItemRectMin();applyMax=ImGui::GetItemRectMax();
        ImGui::SameLine();DrawImagePointAddButton(s);pointMin=ImGui::GetItemRectMin();pointMax=ImGui::GetItemRectMax();
        if(popup){ImGui::OpenPopup("Blocking popup");popup=false;}
        origin=ImGui::GetCursorScreenPos();DrawImageCanvas(s,ImTextureID(1),ImTextureID(2),{700,420});
        if(ImGui::BeginPopup("Blocking popup")){ImGui::TextUnformatted("Popup owns gestures");ImGui::EndPopup();}ImGui::End();ImGui::Render();};
    auto move=[&](float x,float y){io.AddMousePosEvent(x,y);frame();frame();};
    auto click=[&](int button=0){io.AddMouseButtonEvent(button,true);frame();io.AddMouseButtonEvent(button,false);frame();};
    frame();frame();move(origin.x+150,origin.y+210);click();Require(s.protection.selected==7,"Canvas label selection failed");
    move((applyMin.x+applyMax.x)*.5f,(applyMin.y+applyMax.y)*.5f);click();Require(s.protection.weights[7]==1&&s.protection.revision==1,"Actual protection Apply event failed");
    move(origin.x+150,origin.y+210);io.AddMouseWheelEvent(0,2);frame();Require(s.display.zoom>1,"Actual wheel zoom failed");
    const float zoom=s.display.zoom;const auto pan=s.display.panX;io.AddMouseButtonEvent(2,true);frame();io.AddMousePosEvent(origin.x+185,origin.y+220);frame();io.AddMouseButtonEvent(2,false);frame();
    Require(s.display.panX!=pan&&s.lighting.target==original,"MMB pan changed light or failed");
    const auto display=s.display;move(origin.x+650,origin.y+45);io.AddMouseButtonEvent(0,true);frame();io.AddMousePosEvent(origin.x+670,origin.y+55);frame();io.AddMouseButtonEvent(0,false);frame();
    Require(s.lighting.target!=original&&s.lighting.source==original,"Actual sun drag failed or edited source");
    Require(s.display.zoom==display.zoom&&s.display.panX==display.panX,"Sun drag also panned image");
    move((textMin.x+textMax.x)*.5f,(textMin.y+textMax.y)*.5f);click();io.AddInputCharactersUTF8(" /中文/exposure");frame();frame();
    move(origin.x+150,origin.y+210);io.AddMouseWheelEvent(0,1);frame();Require(s.display.zoom==zoom,"Text focus leaked wheel into canvas");
    Require(std::string(text).find("exposure")!=std::string::npos,"Actual text event not consumed");
    io.AddKeyEvent(ImGuiKey_Escape,true);frame();io.AddKeyEvent(ImGuiKey_Escape,false);frame();
    move((sliderMin.x+sliderMax.x)*.5f,(sliderMin.y+sliderMax.y)*.5f);io.AddMouseButtonEvent(0,true);frame();
    move(origin.x+150,origin.y+210);io.AddMouseWheelEvent(0,1);frame();Require(s.display.zoom==zoom,"Slider focus leaked wheel");io.AddMouseButtonEvent(0,false);frame();
    popup=true;frame();frame();io.AddMouseWheelEvent(0,1);frame();Require(s.display.zoom==zoom,"Popup leaked wheel");
    io.AddKeyEvent(ImGuiKey_Escape,true);frame();io.AddKeyEvent(ImGuiKey_Escape,false);frame();
    // An outside press dismisses a popup and must be consumed; start the wipe with a fresh gesture.
    move(origin.x+650,origin.y+380);click();frame();
    Require(!ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId|ImGuiPopupFlags_AnyPopupLevel),"Popup did not dismiss");
    s.display.comparison=2;move(origin.x+300,origin.y+200);io.AddMouseButtonEvent(0,true);frame();io.AddMousePosEvent(origin.x+420,origin.y+200);frame();io.AddMouseButtonEvent(0,false);frame();
    std::cout<<"wipe="<<s.display.wipe<<" popup="<<ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId|ImGuiPopupFlags_AnyPopupLevel)<<" active="<<ImGui::IsAnyItemActive()<<" text="<<io.WantTextInput<<" origin="<<origin.x<<","<<origin.y<<" mouse="<<io.MousePos.x<<","<<io.MousePos.y<<'\n';
    Require(std::abs(s.display.wipe-.6f)<.01f,"Actual wipe drag failed");
    const auto selected=s.protection.selected;s.imageView=ImageDebugView::CastOldMap;
    move(origin.x+450,origin.y+220);click();Require(s.protection.selected==selected&&s.display.comparison==2,"Light-space debug falsely picked a source region or lost comparison state");
    s.display.Fit();s.display.comparison=0;s.imageView=ImageDebugView::Relighted;frame();
    move((pointMin.x+pointMax.x)*.5f,(pointMin.y+pointMax.y)*.5f);click();Require(s.pointEdit.placing,"Image point button did not enter placement");
    move(origin.x+350,origin.y+210);click();Require(s.pointLights.size()==1&&!s.pointEdit.placing,"Image point placement failed");
    const auto point=s.pointLights[0];Require(std::abs(point.position[2]-1.7f)<1e-5,"Source camera depth for new point incorrect");
    const auto sourceLight=s.lighting.source, targetLight=s.lighting.target;const auto savedDisplay=s.display;
    move(origin.x+350,origin.y+210);io.AddMouseButtonEvent(0,true);frame();io.AddMousePosEvent(origin.x+390,origin.y+195);frame();io.AddMouseButtonEvent(0,false);frame();
    Require(s.pointLights[0].position[0]>point.position[0]&&s.pointLights[0].position[1]>point.position[1]&&s.pointLights[0].position[2]==point.position[2],"Image handle drag failed fixed-depth projection");
    Require(s.lighting.source==sourceLight&&s.lighting.target==targetLight&&s.display.panX==savedDisplay.panX&&s.display.zoom==savedDisplay.zoom,"Point drag leaked to sun/pan/zoom");
    move(origin.x+390,origin.y+195);io.AddMouseButtonEvent(0,true);frame();
    io.AddMousePosEvent(origin.x+740,origin.y+210);frame();frame();
    Require(s.pointEdit.dragging==0,"Point drag lost capture outside image");
    io.AddMousePosEvent(origin.x+350,origin.y+210);frame();io.AddMouseButtonEvent(0,false);frame();
    Require(std::abs(s.pointLights[0].position[0])<1e-5&&!s.pointEdit.dragging,"Point outside drag did not return/release");
    Require(!ImagePointAt(s,-.1f,.5f)&&!ImagePointAt(s,1,.5f),"Placement accepted outside source image");
    s.pointEdit.placing=true;io.AddKeyEvent(ImGuiKey_Escape,true);frame();io.AddKeyEvent(ImGuiKey_Escape,false);frame();Require(!s.pointEdit.placing&&s.pointLights.size()==1,"Image Esc cancellation failed");
    s.display.zoom=zoom;
    const auto savedPoints=s.pointLights;
    s.SetMode(WorkMode::Scene3D);s.SetMode(WorkMode::ImageRelighting);Require(s.display.zoom==zoom&&s.protection.weights[7]==1&&s.pointLights==savedPoints,"Mode toggle lost image edits");
    Require(source->analysisMaps->maps[5].image->bytes==sourceBytes,"Editing mutated fixed observation");
    const auto recipe=RecipeState(s);RelightingSession restored;restored.Publish(source);restored.lighting.source=original;
    ApplyRecipeState(restored,recipe);Require(RecipeState(restored)==recipe,"Actual ImGui sun/protection edits did not survive recipe state roundtrip");
    s.Publish(source);Require(s.display.zoom==1&&s.protection.weights.empty()&&s.pointLights.empty(),"Commit did not reset image edit document");
    std::cout<<"Actual ImGui wheel, pan, sun, label selection, protection Apply, text, slider focus, popup and wipe events: PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
