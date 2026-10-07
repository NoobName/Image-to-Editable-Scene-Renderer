#include "Scene/SceneEditing.h"
#include <DirectXCollision.h>
#include <algorithm>
#include <cmath>
#include <cfloat>
#include <stdexcept>
namespace isr {
using namespace DirectX;
namespace {
XMVECTOR Corner(const Bounds& b,unsigned i){return XMVectorSet(i&1?b.maximum.x:b.minimum.x,i&2?b.maximum.y:b.minimum.y,i&4?b.maximum.z:b.minimum.z,1);}
BoundingBox Box(const Bounds& b){return {{(b.minimum.x+b.maximum.x)*.5f,(b.minimum.y+b.maximum.y)*.5f,(b.minimum.z+b.maximum.z)*.5f},
    {(b.maximum.x-b.minimum.x)*.5f,(b.maximum.y-b.minimum.y)*.5f,(b.maximum.z-b.minimum.z)*.5f}};}
}
void SceneEditState::Capture(const Scene& scene){
    Reset();originalMaterials_=scene.materials;originalTransforms_.reserve(scene.entities.size());
    for(const auto& e:scene.entities)originalTransforms_.push_back(e.transform);
    for(const auto& mesh:scene.meshes)bounds_.push_back(MeshBounds(mesh));captured_=true;
}
void SceneEditState::RestoreMaterial(Scene& scene,size_t index)const{scene.materials.at(index)=originalMaterials_.at(index);}
void SceneEditState::RestoreTransform(Scene& scene,size_t index)const{scene.entities.at(index).transform=originalTransforms_.at(index);scene.UpdateWorldMatrices();}
std::optional<Bounds> SceneEditState::SelectionBounds(const Scene& scene,size_t index)const{
    auto lo=XMVectorReplicate(FLT_MAX),hi=-lo;bool found=false;
    for(auto i:scene.RenderableSubtree(index)){const auto& e=scene.entities[i];if(!e.renderer->visible)continue;
        const auto& b=bounds_.at(e.renderer->meshIndex);for(unsigned corner=0;corner<8;++corner){
            const auto p=XMVector3TransformCoord(Corner(b,corner),e.transform.WorldMatrix());lo=XMVectorMin(lo,p);hi=XMVectorMax(hi,p);}
        found=true;
    }
    if(!found)return {};Bounds result;XMStoreFloat3(&result.minimum,lo);XMStoreFloat3(&result.maximum,hi);return result;
}
std::optional<size_t> SceneEditState::Pick(const Scene& scene,float u,float v,XMFLOAT3* hitPosition)const{
    if(!std::isfinite(u+v)||u<0||u>1||v<0||v>1)return {};
    const auto inverse=XMMatrixInverse(nullptr,scene.camera.View()*scene.camera.Projection());
    const auto nearPoint=XMVector3TransformCoord(XMVectorSet(u*2-1,1-v*2,0,1),inverse);
    const auto farPoint=XMVector3TransformCoord(XMVectorSet(u*2-1,1-v*2,1,1),inverse);
    const auto direction=XMVector3Normalize(farPoint-nearPoint);
    float nearest=XMVectorGetX(XMVector3Length(farPoint-nearPoint));std::optional<size_t> hit;
    for(size_t i=0;i<scene.entities.size();++i){const auto& entity=scene.entities[i];if(!entity.renderer||!entity.renderer->visible)continue;
        const auto& renderer=*entity.renderer;const auto world=entity.transform.WorldMatrix(),local=XMMatrixInverse(nullptr,world);
        const auto origin=XMVector3TransformCoord(nearPoint,local);
        const auto ray=XMVector3Normalize(XMVector3TransformNormal(direction,local));float distance=0;
        if(!Box(bounds_.at(renderer.meshIndex)).Intersects(origin,ray,distance))continue;
        const auto& mesh=scene.meshes.at(renderer.meshIndex);const auto& material=scene.materials.at(renderer.materialIndex);
        for(size_t j=0;j+2<mesh.indices.size();j+=3){
            const auto a=XMLoadFloat3(&mesh.vertices.at(mesh.indices[j]).position),b=XMLoadFloat3(&mesh.vertices.at(mesh.indices[j+1]).position),c=XMLoadFloat3(&mesh.vertices.at(mesh.indices[j+2]).position);
            if(!material.doubleSided&&XMVectorGetX(XMVector3Dot(XMVector3Cross(b-a,c-a),ray))>=0)continue;
            if(!TriangleTests::Intersects(origin,ray,a,b,c,distance))continue;
            const auto worldHit=XMVector3TransformCoord(origin+ray*distance,world);
            const auto worldDistance=XMVectorGetX(XMVector3Dot(worldHit-nearPoint,direction));
            if(worldDistance>=0&&worldDistance<nearest){nearest=worldDistance;hit=i;if(hitPosition)XMStoreFloat3(hitPosition,worldHit);}
        }
    }
    if(hit){size_t ancestor=*hit;
        for(;;){if(!scene.entities[ancestor].objectId.empty())return ancestor;
            if(!scene.entities[ancestor].parent)break;ancestor=*scene.entities[ancestor].parent;}}
    return hit;
}
void SceneEditState::EditTransform(Scene& scene,size_t index,Transform next,bool preserveCenter)const{
    scene.UpdateWorldMatrices();const auto& entity=scene.entities.at(index);
    if(preserveCenter)if(auto bounds=SelectionBounds(scene,index)){
        const auto worldCenter=(XMLoadFloat3(&bounds->minimum)+XMLoadFloat3(&bounds->maximum))*.5f;
        const auto localCenter=XMVector3TransformCoord(worldCenter,XMMatrixInverse(nullptr,entity.transform.WorldMatrix()));
        const auto before=XMVector3TransformCoord(localCenter,entity.transform.LocalMatrix());
        const auto after=XMVector3TransformCoord(localCenter,next.LocalMatrix());
        // Translation precedes importedLocal in this project's offset convention.
        const auto correction=next.importedLocal?XMVector3TransformNormal(before-after,XMMatrixInverse(nullptr,XMLoadFloat4x4(&*next.importedLocal))):before-after;
        XMStoreFloat3(&next.position,XMLoadFloat3(&next.position)+correction);
    }
    (void)next.LocalMatrix();scene.entities.at(index).transform=next;scene.UpdateWorldMatrices();
}
XMFLOAT3 DragSunDirection(XMFLOAT3 direction,const Camera& camera,float dx,float dy){
    const auto d=XMLoadFloat3(&direction);
    if(!std::isfinite(direction.x+direction.y+direction.z+dx+dy)||XMVectorGetX(XMVector3LengthSq(d))<1e-10f)
        throw std::invalid_argument("Sun direction/drag must be finite and nonzero");
    auto sun=XMVector3TransformNormal(-XMVector3Normalize(d),camera.View());
    sun=XMVector3TransformNormal(sun,XMMatrixRotationY(dx*.012f)*XMMatrixRotationX(dy*.012f));
    sun=XMVector3TransformNormal(sun,XMMatrixInverse(nullptr,camera.View()));
    XMFLOAT3 result;XMStoreFloat3(&result,-XMVector3Normalize(sun));return result;
}
}
