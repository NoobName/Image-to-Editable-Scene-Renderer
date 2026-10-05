#include "UI/ScenePanels.h"
#include "Scene/Bounds.h"
#include "Core/Log.h"
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
namespace isr {
using namespace DirectX;
namespace {
constexpr auto Clamp=ImGuiSliderFlags_AlwaysClamp;
void MaterialControls(Scene& scene,size_t index,SceneEditState& edit){
    auto& m=scene.materials.at(index);const auto& original=edit.OriginalMaterial(index);
    ImGui::TextWrapped("%s",m.name.c_str());
    if(ImGui::TreeNode("Original Material")){
        ImGui::TextWrapped("Parameters at scene load. For reconstructed scenes these are the estimated material, not the input photograph.");
        ImGui::ColorButton("Original Base Color",ImVec4(original.baseColor.x,original.baseColor.y,original.baseColor.z,original.baseColor.w),ImGuiColorEditFlags_NoTooltip);
        ImGui::SameLine();ImGui::Text("Base Color %.2f %.2f %.2f",original.baseColor.x,original.baseColor.y,original.baseColor.z);
        ImGui::Text("Roughness %.3f",original.roughness);ImGui::Text("Metallic %.3f",original.metallic);
        ImGui::Text("Normal Strength %.2f",original.normalScale);ImGui::TreePop();
    }
    if(m.albedoSource!="authored"&&ImGui::TreeNode("Material sources")) {
        ImGui::TextWrapped("Albedo source: %s",m.albedoSource.c_str());
        ImGui::TextWrapped("Normal source: %s",m.normalSource.c_str());
        ImGui::TextWrapped("Estimated views show raw maps. Final uses your material factors.");
        ImGui::TreePop();
    }
    const auto users=std::count_if(scene.entities.begin(),scene.entities.end(),[&](const Entity& e){return e.renderer&&e.renderer->materialIndex==index;});
    if(users>1)ImGui::TextWrapped("Shared material: edits affect %zu primitives.",static_cast<size_t>(users));
    ImGui::TextWrapped("Base Color tints the texture. Enable the override to replace roughness / metallic maps with constants.");
    bool changed=ImGui::Checkbox("Override roughness / metallic maps",&m.overrideMetallicRoughness);
    if(ImGui::IsItemHovered())ImGui::SetTooltip("Without override, sliders multiply texture values (1 preserves the map).\nA zero metallic texel stays zero. Override allows any constant metallic value.");
    changed|=ImGui::ColorEdit4("Base Color",&m.baseColor.x,ImGuiColorEditFlags_Float);
    changed|=ImGui::SliderFloat("Metallic",&m.metallic,0,1,"%.3f");
    changed|=ImGui::SliderFloat("Roughness",&m.roughness,0,1,"%.3f");
    changed|=ImGui::SliderFloat("Normal Strength",&m.normalScale,0,2,"%.2f");
    changed|=ImGui::SliderFloat("AO",&m.ao,0,1,"%.3f");
    changed|=ImGui::SliderFloat("AO Map Strength",&m.occlusionStrength,0,1,"%.3f");
    changed|=ImGui::ColorEdit3("Emissive",&m.emissive.x,ImGuiColorEditFlags_Float|ImGuiColorEditFlags_HDR);
    if(ImGui::TreeNode("Texture bindings")){
        const char* roles[]={"Base Color / sRGB","Metal-Rough / Linear","Normal / Linear","Emissive / sRGB","AO / Linear","Original Image / sRGB"};
        for(size_t i=0;i<MaterialTextureCount;++i)ImGui::BulletText("%s: %s",roles[i],m.textures[i].texture?"loaded":"fallback");
        ImGui::TreePop();
    }
    if(m.unlit)ImGui::TextWrapped("This material is unlit.");
    if(ImGui::Button("Restore original material")){edit.RestoreMaterial(scene,index);changed=true;}
    if(changed)Log("Material edited: "+m.name+" metallic="+std::to_string(m.metallic)+" roughness="+std::to_string(m.roughness));
}
}
void DrawEntityInspector(Scene& scene,size_t index,SceneEditState& edit,RenderSettings& settings){
    if(index>=scene.entities.size()){ImGui::TextUnformatted("No mesh selected");return;}
    auto& entity=scene.entities[index];ImGui::TextWrapped("%s",entity.name.c_str());
    if(!entity.objectId.empty())ImGui::TextWrapped("Object ID: %s",entity.objectId.c_str());
    if(entity.region&&ImGui::CollapsingHeader("Region")) {
        const auto& r=*entity.region;ImGui::Text("Category: %s",r.category.c_str());ImGui::Text("Naming: %s",r.namingSource.c_str());
        ImGui::Text("Mask label: %u / %u pixels",r.labelId,r.pixelCount);
        ImGui::Text("Box: %u, %u / %u x %u",r.boundingBox[0],r.boundingBox[1],r.boundingBox[2],r.boundingBox[3]);
        if(r.averageDepth)ImGui::Text("Average source depth: %.3f m",*r.averageDepth);else ImGui::TextUnformatted("Average depth: unavailable");
        ImGui::Text("Source triangles: %u",r.triangleCount);
    }
    if(ImGui::CollapsingHeader("Transform",ImGuiTreeNodeFlags_DefaultOpen)){
        auto edited=entity.transform;const bool trs=edited.TryDecomposeImported();
        ImGui::TextDisabled(trs?"Local space / rotation in degrees":"Local offset over imported matrix");
        if(!trs)ImGui::TextWrapped("The original matrix contains shear; it is preserved under this editable offset.");
        bool changed=ImGui::DragFloat3("Position",&edited.position.x,0.02f,-100000,100000,"%.3f",Clamp);
        bool pivotChange=false;
        XMFLOAT3 degrees{XMConvertToDegrees(edited.rotation.x),XMConvertToDegrees(edited.rotation.y),XMConvertToDegrees(edited.rotation.z)};
        if(ImGui::DragFloat3("Rotation",&degrees.x,0.5f,-36000,36000,"%.1f",Clamp)){
            edited.rotation={XMConvertToRadians(degrees.x),XMConvertToRadians(degrees.y),XMConvertToRadians(degrees.z)};changed=pivotChange=true;
        }
        if(ImGui::DragFloat3("Scale",&edited.scale.x,0.01f,-10000,10000,"%.3f",Clamp)){
            for(float* v:{&edited.scale.x,&edited.scale.y,&edited.scale.z})if(std::abs(*v)<0.001f)*v=std::copysign(0.001f,*v);
            changed=pivotChange=true;
        }
        ImGui::Checkbox("Rotate / scale around center",&edit.centerPivot);
        if(changed){edit.EditTransform(scene,index,edited,pivotChange&&edit.centerPivot);Log("Transform edited: "+entity.name);}
        if(ImGui::Button("Reset transform"))edit.RestoreTransform(scene,index);
    }
    const auto renderers=scene.RenderableSubtree(index);
    if(!renderers.empty()) {
        bool visible=std::any_of(renderers.begin(),renderers.end(),[&](size_t i){return scene.entities[i].renderer->visible;});
        if(ImGui::Checkbox("Visible",&visible))for(auto i:renderers)scene.entities[i].renderer->visible=visible;
        std::set<size_t> materials;for(auto i:renderers)materials.insert(scene.entities[i].renderer->materialIndex);
        if(ImGui::CollapsingHeader("Material",ImGuiTreeNodeFlags_DefaultOpen)){
            if(settings.mode==RenderMode::OriginalImage||settings.mode==RenderMode::EstimatedAlbedo||settings.mode==RenderMode::EstimatedNormal||settings.mode==RenderMode::EstimatedRoughness){
                ImGui::TextWrapped("This view shows raw source maps. Switch to Final to see material and lighting edits.");
                if(ImGui::Button("Show Final"))settings.mode=RenderMode::Final;
            }
            for(auto material:materials) {
            ImGui::PushID(static_cast<int>(material));MaterialControls(scene,material,edit);ImGui::PopID();}
        }
    }else ImGui::TextWrapped(entity.region?"This region has no valid mesh triangles. Its mask and identity are retained.":"This node contains no renderable primitives.");
}
void DrawCameraInspector(Camera& camera){
    ImGui::TextUnformatted("Perspective Camera");
    auto position=camera.Position();auto angles=camera.Angles();angles.x=XMConvertToDegrees(angles.x);angles.y=XMConvertToDegrees(angles.y);
    bool changed=ImGui::DragFloat3("Position",&position.x,0.05f,-100000,100000,"%.3f",Clamp);
    changed|=ImGui::DragFloat("Yaw",&angles.x,0.5f,-36000,36000,"%.1f deg",Clamp);
    changed|=ImGui::SliderFloat("Pitch",&angles.y,-89,89,"%.1f deg");
    if(changed)camera.SetPose(position,XMConvertToRadians(angles.x),XMConvertToRadians(angles.y));
    float fov=XMConvertToDegrees(camera.Fov()),nearPlane=camera.NearPlane(),farPlane=camera.FarPlane();
    changed=ImGui::SliderFloat("FOV",&fov,10,120,"%.1f deg");
    changed|=ImGui::DragFloat("Near",&nearPlane,0.01f,0.001f,farPlane-0.001f,"%.3f",Clamp);
    changed|=ImGui::DragFloat("Far",&farPlane,1,nearPlane+0.001f,1000000,"%.1f",Clamp);
    if(changed)camera.SetPerspective(XMConvertToRadians(fov),camera.Aspect(),nearPlane,farPlane);
    ImGui::Text("Aspect %.3f",camera.Aspect());
}
void DrawLightInspector(Light& light,RenderSettings& settings){
    ImGui::TextUnformatted(light.type==LightType::Point?"Point Light":"Directional Light");
    ImGui::ColorEdit3("Color",&light.color.x,ImGuiColorEditFlags_Float);
    ImGui::DragFloat("Intensity",&light.intensity,0.05f,0,10000,"%.2f",Clamp);
    if(light.type==LightType::Directional){
        auto direction=light.direction;
        if(ImGui::DragFloat3("Direction",&direction.x,0.01f,-1,1,"%.3f",Clamp)){
            const auto v=XMLoadFloat3(&direction);if(XMVectorGetX(XMVector3LengthSq(v))>1e-8f)XMStoreFloat3(&light.direction,XMVector3Normalize(v));
        }
        ImGui::TextWrapped("Direction in which the light travels. A zero vector is ignored.");
        if(ImGui::CollapsingHeader("Shadow",ImGuiTreeNodeFlags_DefaultOpen)){
            ImGui::Checkbox("Shadows",&settings.shadows);
            const char* filters[]={"Hard","PCF 3x3","PCF 5x5"};ImGui::Combo("Filter",&settings.shadowPcfRadius,filters,3);
            ImGui::SliderFloat("Shadow Bias",&settings.shadowBias,0,0.01f,"%.5f");
            ImGui::SliderFloat("Normal Bias",&settings.shadowNormalBias,0,0.15f,"%.3f");
            ImGui::TextWrapped("2048 x 2048; first active directional light casts shadows.");
        }
    }else{
        ImGui::DragFloat3("Position",&light.position.x,0.05f,-100000,100000,"%.3f",Clamp);
        ImGui::DragFloat("Range",&light.range,0.1f,0.01f,10000,"%.2f",Clamp);
    }
}
bool FrameScene(Scene& scene,const SceneSelection* selection){
    scene.UpdateWorldMatrices();XMVECTOR lo=XMVectorReplicate(FLT_MAX),hi=XMVectorReplicate(-FLT_MAX);bool found=false;
    for(size_t i=0;i<scene.entities.size();++i){const auto& entity=scene.entities[i];
        if(!entity.renderer||!entity.renderer->visible)continue;
        if(selection&&selection->kind==SelectionKind::Entity){
            size_t ancestor=i;while(ancestor!=selection->index&&scene.entities[ancestor].parent)ancestor=*scene.entities[ancestor].parent;
            if(ancestor!=selection->index)continue;
        }
        const auto b=MeshBounds(scene.meshes.at(entity.renderer->meshIndex));
        for(unsigned corner=0;corner<8;++corner){const auto p=XMVector3TransformCoord(XMVectorSet(
            (corner&1)?b.maximum.x:b.minimum.x,(corner&2)?b.maximum.y:b.minimum.y,(corner&4)?b.maximum.z:b.minimum.z,1),entity.transform.WorldMatrix());
            lo=XMVectorMin(lo,p);hi=XMVectorMax(hi,p);}found=true;
    }
    if(!found)return false;
    const auto center=(lo+hi)*0.5f;const float radius=std::max(0.05f,XMVectorGetX(XMVector3Length(hi-lo))*0.5f);
    const float halfFov=std::atan(std::tan(scene.camera.Fov()*0.5f)*std::min(1.0f,scene.camera.Aspect()));
    const float distance=radius/std::sin(halfFov)*1.1f;
    const auto view=XMMatrixInverse(nullptr,scene.camera.View());const auto direction=-view.r[2];
    XMFLOAT3 eye,target;XMStoreFloat3(&eye,center+direction*distance);XMStoreFloat3(&target,center);
    scene.camera.LookAt(eye,target);
    scene.camera.SetPerspective(scene.camera.Fov(),scene.camera.Aspect(),std::max(0.001f,std::min(0.1f,radius*0.01f)),std::max(200.0f,distance+radius*3));
    return true;
}
}
