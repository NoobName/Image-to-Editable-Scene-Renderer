#include "Scene/Bounds.h"
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <cmath>
namespace isr {
using namespace DirectX;
namespace {
void Extend(XMVECTOR& lo,XMVECTOR& hi,FXMVECTOR p){lo=XMVectorMin(lo,p);hi=XMVectorMax(hi,p);}
Bounds Store(FXMVECTOR lo,FXMVECTOR hi){Bounds b;XMStoreFloat3(&b.minimum,lo);XMStoreFloat3(&b.maximum,hi);return b;}
XMVECTOR Corner(const Bounds& b,unsigned i){return XMVectorSet(i&1?b.maximum.x:b.minimum.x,i&2?b.maximum.y:b.minimum.y,i&4?b.maximum.z:b.minimum.z,1);}
}
Bounds MeshBounds(const Mesh& mesh){
    if(mesh.vertices.empty())throw std::invalid_argument("Cannot bound an empty mesh");
    auto lo=XMVectorReplicate(FLT_MAX),hi=-lo;
    for(const auto& v:mesh.vertices)Extend(lo,hi,XMLoadFloat3(&v.position));return Store(lo,hi);
}
Bounds WorldBounds(const Scene& scene,const std::vector<Bounds>& meshes){
    auto lo=XMVectorReplicate(FLT_MAX),hi=-lo;bool any=false;
    for(const auto& e:scene.entities)if(e.renderer&&e.renderer->visible){any=true;
        for(unsigned i=0;i<8;++i)Extend(lo,hi,XMVector3TransformCoord(Corner(meshes.at(e.renderer->meshIndex),i),e.transform.WorldMatrix()));}
    return any?Store(lo,hi):Bounds{{-1,-1,-1},{1,1,1}};
}
XMMATRIX FitDirectionalShadow(const Bounds& bounds,XMFLOAT3 direction,unsigned resolution){
    auto d=XMLoadFloat3(&direction);if(!resolution||XMVectorGetX(XMVector3LengthSq(d))<1e-10f)throw std::invalid_argument("Invalid shadow direction/resolution");
    d=XMVector3Normalize(d);auto lo=XMLoadFloat3(&bounds.minimum),hi=XMLoadFloat3(&bounds.maximum);
    const auto center=(lo+hi)*0.5f;float radius=std::max(0.1f,XMVectorGetX(XMVector3Length(hi-lo))*0.5f);
    auto up=std::abs(XMVectorGetY(d))>0.95f?XMVectorSet(1,0,0,0):XMVectorSet(0,1,0,0);
    auto view=XMMatrixLookToLH(center-d*(radius*2+1),d,up);
    lo=XMVectorReplicate(FLT_MAX);hi=-lo;
    for(unsigned i=0;i<8;++i)Extend(lo,hi,XMVector3TransformCoord(Corner(bounds,i),view));
    auto b=Store(lo,hi);const float padding=std::max(radius*0.05f,0.05f);
    float width=b.maximum.x-b.minimum.x+padding*2,height=b.maximum.y-b.minimum.y+padding*2;
    float cx=(b.minimum.x+b.maximum.x)*0.5f,cy=(b.minimum.y+b.maximum.y)*0.5f;
    cx=std::floor(cx/(width/resolution))*(width/resolution);cy=std::floor(cy/(height/resolution))*(height/resolution);
    return view*XMMatrixOrthographicOffCenterLH(cx-width/2,cx+width/2,cy-height/2,cy+height/2,
        std::max(0.01f,b.minimum.z-padding),b.maximum.z+padding);
}
}
