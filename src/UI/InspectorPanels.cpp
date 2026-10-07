#include "UI/ChineseText.h"
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
    ImGui::TextWrapped("%s",ChineseText(m.name).c_str());
    if(ImGui::TreeNode("原始材质###Original Material")){
        ImGui::TextWrapped("场景加载时的材质参数。对重建场景而言，这是估计材质，而不是输入照片。");
        ImGui::ColorButton("原始基础颜色###Original Base Color",ImVec4(original.baseColor.x,original.baseColor.y,original.baseColor.z,original.baseColor.w),ImGuiColorEditFlags_NoTooltip);
        ImGui::SameLine();ImGui::Text("基础颜色 %.2f %.2f %.2f",original.baseColor.x,original.baseColor.y,original.baseColor.z);
        ImGui::Text("粗糙度 %.3f",original.roughness);ImGui::Text("金属度 %.3f",original.metallic);
        ImGui::Text("法线强度 %.2f",original.normalScale);ImGui::TreePop();
    }
    if(m.albedoSource!="authored"&&ImGui::TreeNode("材质来源###Material sources")) {
        ImGui::TextWrapped("反照率来源：%s",ChineseText(m.albedoSource).c_str());
        ImGui::TextWrapped("法线来源：%s",ChineseText(m.normalSource).c_str());
        ImGui::TextWrapped("估计视图显示原始贴图；最终效果使用当前材质因子。");
        ImGui::TreePop();
    }
    const auto users=std::count_if(scene.entities.begin(),scene.entities.end(),[&](const Entity& e){return e.renderer&&e.renderer->materialIndex==index;});
    if(users>1)ImGui::TextWrapped("共享材质：修改会影响 %zu 个图元。",static_cast<size_t>(users));
    ImGui::TextWrapped("基础颜色对纹理进行染色。启用覆盖后，可用常量替换粗糙度和金属度贴图。");
    bool changed=ImGui::Checkbox("覆盖粗糙度 / 金属度贴图###Override roughness / metallic maps",&m.overrideMetallicRoughness);
    if(ImGui::IsItemHovered())ImGui::SetTooltip("未覆盖时，滑块值与纹理相乘，1 表示保留贴图。\n金属度为 0 的像素仍为 0；覆盖后可设置任意金属度常量。");
    changed|=ImGui::ColorEdit4("基础颜色###Base Color",&m.baseColor.x,ImGuiColorEditFlags_Float);
    changed|=ImGui::SliderFloat("金属度###Metallic",&m.metallic,0,1,"%.3f");
    changed|=ImGui::SliderFloat("粗糙度###Roughness",&m.roughness,0,1,"%.3f");
    changed|=ImGui::SliderFloat("法线强度###Normal Strength",&m.normalScale,0,2,"%.2f");
    changed|=ImGui::SliderFloat("环境遮蔽###AO",&m.ao,0,1,"%.3f");
    changed|=ImGui::SliderFloat("环境遮蔽贴图强度###AO Map Strength",&m.occlusionStrength,0,1,"%.3f");
    changed|=ImGui::ColorEdit3("自发光###Emissive",&m.emissive.x,ImGuiColorEditFlags_Float|ImGuiColorEditFlags_HDR);
    if(ImGui::TreeNode("纹理绑定###Texture bindings")){
        const char* roles[]={"基础颜色 / sRGB","金属度与粗糙度 / 线性","法线 / 线性","自发光 / sRGB","环境遮蔽 / 线性","原图 / sRGB"};
        for(size_t i=0;i<MaterialTextureCount;++i)ImGui::BulletText("%s: %s",roles[i],m.textures[i].texture?"已加载":"回退值");
        ImGui::TreePop();
    }
    if(m.unlit)ImGui::TextWrapped("此材质不受光照影响。");
    if(ImGui::Button("恢复原始材质###Restore original material")){edit.RestoreMaterial(scene,index);changed=true;}
    if(changed)Log("Material edited: "+m.name+" metallic="+std::to_string(m.metallic)+" roughness="+std::to_string(m.roughness));
}
}
void DrawEntityInspector(Scene& scene,size_t index,SceneEditState& edit,RenderSettings& settings){
    if(index>=scene.entities.size()){ImGui::TextUnformatted("未选择网格");return;}
    auto& entity=scene.entities[index];ImGui::TextWrapped("%s",ChineseText(entity.name).c_str());
    if(!entity.objectId.empty())ImGui::TextWrapped("物体标识：%s",entity.objectId.c_str());
    if(entity.region&&ImGui::CollapsingHeader("区域###Region")) {
        const auto& r=*entity.region;ImGui::Text("类别：%s",ChineseText(r.category).c_str());ImGui::Text("命名来源：%s",ChineseText(r.namingSource).c_str());
        ImGui::Text("蒙版标签：%u / %u 像素",r.labelId,r.pixelCount);
        ImGui::Text("包围框：%u，%u / %u × %u",r.boundingBox[0],r.boundingBox[1],r.boundingBox[2],r.boundingBox[3]);
        if(r.averageDepth)ImGui::Text("原始平均深度：%.3f 米",*r.averageDepth);else ImGui::TextUnformatted("平均深度：不可用");
        ImGui::Text("来源三角形：%u",r.triangleCount);
    }
    if(ImGui::CollapsingHeader("变换###Transform",ImGuiTreeNodeFlags_DefaultOpen)){
        auto edited=entity.transform;const bool trs=edited.TryDecomposeImported();
        ImGui::TextDisabled(trs?"局部空间 / 旋转单位为度":"在导入矩阵上叠加局部偏移");
        if(!trs)ImGui::TextWrapped("原矩阵包含错切，会在可编辑偏移下保留。");
        bool changed=ImGui::DragFloat3("位置###Position",&edited.position.x,0.02f,-100000,100000,"%.3f",Clamp);
        bool pivotChange=false;
        XMFLOAT3 degrees{XMConvertToDegrees(edited.rotation.x),XMConvertToDegrees(edited.rotation.y),XMConvertToDegrees(edited.rotation.z)};
        if(ImGui::DragFloat3("旋转###Rotation",&degrees.x,0.5f,-36000,36000,"%.1f",Clamp)){
            edited.rotation={XMConvertToRadians(degrees.x),XMConvertToRadians(degrees.y),XMConvertToRadians(degrees.z)};changed=pivotChange=true;
        }
        if(ImGui::DragFloat3("缩放###Scale",&edited.scale.x,0.01f,-10000,10000,"%.3f",Clamp)){
            for(float* v:{&edited.scale.x,&edited.scale.y,&edited.scale.z})if(std::abs(*v)<0.001f)*v=std::copysign(0.001f,*v);
            changed=pivotChange=true;
        }
        ImGui::Checkbox("绕中心旋转 / 缩放###Rotate / scale around center",&edit.centerPivot);
        if(changed){edit.EditTransform(scene,index,edited,pivotChange&&edit.centerPivot);Log("Transform edited: "+entity.name);}
        if(ImGui::Button("重置变换###Reset transform"))edit.RestoreTransform(scene,index);
    }
    const auto renderers=scene.RenderableSubtree(index);
    if(!renderers.empty()) {
        bool visible=std::any_of(renderers.begin(),renderers.end(),[&](size_t i){return scene.entities[i].renderer->visible;});
        if(ImGui::Checkbox("可见###Visible",&visible))for(auto i:renderers)scene.entities[i].renderer->visible=visible;
        std::set<size_t> materials;for(auto i:renderers)materials.insert(scene.entities[i].renderer->materialIndex);
        if(ImGui::CollapsingHeader("材质###Material",ImGuiTreeNodeFlags_DefaultOpen)){
            if(settings.mode==RenderMode::OriginalImage||settings.mode==RenderMode::EstimatedAlbedo||settings.mode==RenderMode::EstimatedNormal||settings.mode==RenderMode::EstimatedRoughness){
                ImGui::TextWrapped("当前显示原始贴图。切换到最终效果以查看材质和光照修改。");
                if(ImGui::Button("显示最终效果###Show Final"))settings.mode=RenderMode::Final;
            }
            for(auto material:materials) {
            ImGui::PushID(static_cast<int>(material));MaterialControls(scene,material,edit);ImGui::PopID();}
        }
    }else ImGui::TextWrapped(entity.region?"此区域没有有效网格三角形；蒙版和身份信息仍保留。":"此节点没有可渲染图元。");
}
void DrawCameraInspector(Camera& camera){
    ImGui::TextUnformatted("透视相机");
    auto position=camera.Position();auto angles=camera.Angles();angles.x=XMConvertToDegrees(angles.x);angles.y=XMConvertToDegrees(angles.y);
    bool changed=ImGui::DragFloat3("位置###Position",&position.x,0.05f,-100000,100000,"%.3f",Clamp);
    changed|=ImGui::DragFloat("偏航角###Yaw",&angles.x,0.5f,-36000,36000,"%.1f 度",Clamp);
    changed|=ImGui::SliderFloat("俯仰角###Pitch",&angles.y,-89,89,"%.1f 度");
    if(changed)camera.SetPose(position,XMConvertToRadians(angles.x),XMConvertToRadians(angles.y));
    float fov=XMConvertToDegrees(camera.Fov()),nearPlane=camera.NearPlane(),farPlane=camera.FarPlane();
    changed=ImGui::SliderFloat("视场角###FOV",&fov,10,120,"%.1f 度");
    changed|=ImGui::DragFloat("近裁剪面###Near",&nearPlane,0.01f,0.001f,farPlane-0.001f,"%.3f",Clamp);
    changed|=ImGui::DragFloat("远裁剪面###Far",&farPlane,1,nearPlane+0.001f,1000000,"%.1f",Clamp);
    if(changed)camera.SetPerspective(XMConvertToRadians(fov),camera.Aspect(),nearPlane,farPlane);
    ImGui::Text("宽高比 %.3f",camera.Aspect());
}
void DrawLightInspector(Light& light,RenderSettings& settings){
    ImGui::TextUnformatted(light.type==LightType::Point?"点光源":"方向光");
    ImGui::ColorEdit3("颜色###Color",&light.color.x,ImGuiColorEditFlags_Float);
    ImGui::DragFloat("强度###Intensity",&light.intensity,0.05f,0,10000,"%.2f",Clamp);
    if(light.type==LightType::Directional){
        auto direction=light.direction;
        if(ImGui::DragFloat3("方向###Direction",&direction.x,0.01f,-1,1,"%.3f",Clamp)){
            const auto v=XMLoadFloat3(&direction);if(XMVectorGetX(XMVector3LengthSq(v))>1e-8f)XMStoreFloat3(&light.direction,XMVector3Normalize(v));
        }
        ImGui::TextWrapped("光线传播的方向，零向量会被忽略。");
        if(ImGui::CollapsingHeader("阴影###Shadow",ImGuiTreeNodeFlags_DefaultOpen)){
            ImGui::Checkbox("启用阴影###Shadows",&settings.shadows);
            const char* filters[]={"硬阴影","PCF 3×3 滤波","PCF 5×5 滤波"};ImGui::Combo("滤波方式###Filter",&settings.shadowPcfRadius,filters,3);
            ImGui::SliderFloat("阴影深度偏移###Shadow Bias",&settings.shadowBias,0,0.01f,"%.5f");
            ImGui::SliderFloat("法线偏移###Normal Bias",&settings.shadowNormalBias,0,0.15f,"%.3f");
            ImGui::TextWrapped("阴影贴图为 2048 × 2048；第一个启用的方向光投射阴影。");
        }
    }else{
        ImGui::DragFloat3("位置###Position",&light.position.x,0.05f,-100000,100000,"%.3f",Clamp);
        ImGui::DragFloat("范围###Range",&light.range,0.1f,0.01f,10000,"%.2f",Clamp);
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
