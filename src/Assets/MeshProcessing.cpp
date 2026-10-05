#include "Assets/MeshProcessing.h"
#include <mikktspace.h>
#include <numeric>
#include <stdexcept>
namespace isr {
using namespace DirectX;
void GenerateNormals(Mesh& mesh) {
    // glTF specifies flat normals when NORMAL is absent. Split shared corners first.
    std::vector<Vertex> corners;corners.reserve(mesh.indices.size());
    for(auto index:mesh.indices)corners.push_back(mesh.vertices.at(index));
    mesh.vertices=std::move(corners);std::iota(mesh.indices.begin(),mesh.indices.end(),0u);
    for (auto& v:mesh.vertices) v.normal={0,0,0};
    for (size_t i=0;i<mesh.indices.size();i+=3) {
        auto& a=mesh.vertices.at(mesh.indices[i]); auto& b=mesh.vertices.at(mesh.indices[i+1]); auto& c=mesh.vertices.at(mesh.indices[i+2]);
        const auto n=XMVector3Cross(XMLoadFloat3(&b.position)-XMLoadFloat3(&a.position),XMLoadFloat3(&c.position)-XMLoadFloat3(&a.position));
        for (auto* v:{&a,&b,&c}) XMStoreFloat3(&v->normal,XMLoadFloat3(&v->normal)+n);
    }
    for (auto& v:mesh.vertices) {
        const auto n=XMLoadFloat3(&v.normal);
        if (XMVectorGetX(XMVector3LengthSq(n))<1e-15f) v.normal={0,1,0}; else XMStoreFloat3(&v.normal,XMVector3Normalize(n));
    }
}
void GenerateTangents(Mesh& mesh,unsigned texCoord) {
    // MikkTSpace returns per-corner results. Splitting corners preserves mirrored UV seams.
    std::vector<Vertex> expanded; expanded.reserve(mesh.indices.size());
    for (auto i:mesh.indices) expanded.push_back(mesh.vertices.at(i));
    mesh.vertices=std::move(expanded); std::iota(mesh.indices.begin(),mesh.indices.end(),0u);
    struct Data { Mesh* mesh; unsigned uv; } data{&mesh,texCoord};
    // Noncapturing callbacks are required by the C interface.
    SMikkTSpaceInterface api{};
    api.m_getNumFaces=[](const SMikkTSpaceContext* c){return int(static_cast<Data*>(c->m_pUserData)->mesh->indices.size()/3);};
    api.m_getNumVerticesOfFace=[](const SMikkTSpaceContext*,int){return 3;};
    api.m_getPosition=[](const SMikkTSpaceContext* c,float out[],int f,int v){auto& p=static_cast<Data*>(c->m_pUserData)->mesh->vertices[size_t(f)*3+v].position;out[0]=p.x;out[1]=p.y;out[2]=p.z;};
    api.m_getNormal=[](const SMikkTSpaceContext* c,float out[],int f,int v){auto& n=static_cast<Data*>(c->m_pUserData)->mesh->vertices[size_t(f)*3+v].normal;out[0]=n.x;out[1]=n.y;out[2]=n.z;};
    api.m_getTexCoord=[](const SMikkTSpaceContext* c,float out[],int f,int v){auto* d=static_cast<Data*>(c->m_pUserData);auto& p=d->mesh->vertices[size_t(f)*3+v];auto uv=d->uv?p.texcoord1:p.texcoord;out[0]=uv.x;out[1]=uv.y;};
    api.m_setTSpaceBasic=[](const SMikkTSpaceContext* c,const float t[],float sign,int f,int v){static_cast<Data*>(c->m_pUserData)->mesh->vertices[size_t(f)*3+v].tangent={t[0],t[1],t[2],sign};};
    SMikkTSpaceContext context{&api,&data};
    if (!genTangSpaceDefault(&context)) throw std::runtime_error("MikkTSpace tangent generation failed");
}
}
