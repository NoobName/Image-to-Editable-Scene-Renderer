#include "App/EditorSmoke.h"
#include "Core/Log.h"
#include "Renderer/ShaderCompiler.h"
#include <algorithm>
#include <set>
#include <stdexcept>
namespace isr {
void EditorSmoke::Initialize(const Scene& scene){
    if(mode.empty())return;
    const std::string_view modes[]={"baseline","move","rotate","scale","material","roughness","metallic","normal","sun","light","environment","exposure","restore-material","restore-transform"};
    if(std::find(std::begin(modes),std::end(modes),mode)==std::end(modes))throw std::invalid_argument("Unknown editor smoke mode");
    for(size_t i=0;i<scene.entities.size();++i){const auto& e=scene.entities[i];
        if((objectId.empty()?bool(e.renderer):e.objectId==objectId)){root_=i;break;}}
    if(!root_)throw std::invalid_argument("Editor smoke object was not found");
    original_.Capture(scene);Log("Editor target: "+scene.entities[*root_].name+" id="+scene.entities[*root_].objectId);
}
void EditorSmoke::Tick(unsigned frame,Scene& scene,RenderSettings& settings){
    if(mode.empty()||!root_||(frame!=30&&frame!=70))return;
    std::set<size_t> materials;for(auto i:scene.RenderableSubtree(*root_))materials.insert(scene.entities[i].renderer->materialIndex);
    if(frame==70){
        if(mode=="restore-material")for(auto i:materials)original_.RestoreMaterial(scene,i);
        else if(mode=="restore-transform")original_.RestoreTransform(scene,*root_);
        else return;
    }else if(mode=="move"||mode=="rotate"||mode=="scale"||mode=="restore-transform"){
        auto next=scene.entities[*root_].transform;
        if(mode=="move")next.position.x+=.65f;
        if(mode=="rotate"||mode=="restore-transform")next.rotation.y+=.5f;
        if(mode=="scale"||mode=="restore-transform")next.scale={.7f,.7f,.7f};
        original_.EditTransform(scene,*root_,next,mode!="move");
    }else if(mode=="material"||mode=="roughness"||mode=="metallic"||mode=="normal"||mode=="restore-material"){
        for(auto i:materials){auto& material=scene.materials[i];
            if(mode=="material"||mode=="restore-material"){material.baseColor={.12f,.85f,.25f,1};material.roughness=.18f;material.metallic=.9f;material.normalScale=0;material.overrideMetallicRoughness=true;}
            if(mode=="roughness"){material.overrideMetallicRoughness=true;material.roughness=.18f;}
            if(mode=="metallic"){material.overrideMetallicRoughness=true;material.metallic=.9f;}
            if(mode=="normal")material.normalScale=0;
        }
    }else if(mode=="sun"||mode=="light"){
        auto sun=std::find_if(scene.lights.begin(),scene.lights.end(),[](const auto& l){return l.type==LightType::Directional;});
        if(sun==scene.lights.end())throw std::invalid_argument("Editor smoke requires a directional light");
        if(mode=="sun")sun->direction=DragSunDirection(sun->direction,scene.camera,70,-30);
        else {sun->color={1,.4f,.15f};sun->intensity=5;}
    }else if(mode=="environment"){
        settings.environmentPath=ExecutableDirectory()/"assets/environments/SunsetCourtyard.hdr";
        settings.ibl=true;settings.environmentIntensity=1.6f;settings.environmentRotation=.9f;
    }else if(mode=="exposure")settings.look.exposure=std::min(16.f,settings.look.exposure+1.5f);
    else return;
    Log("Editor smoke: "+mode+" frame="+std::to_string(frame));
}
}
