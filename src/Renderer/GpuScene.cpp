#include "Renderer/GpuScene.h"
#include <cstddef>
namespace isr {
using namespace DirectX;
GpuScene::GpuScene(ID3D12Device* device,ID3D12GraphicsCommandList* list,DescriptorAllocator& srv,DescriptorAllocator& samplers,const Scene& scene){
    for(const auto& mesh:scene.meshes){UploadedMesh gpu;
        gpu.vertices=std::make_unique<GpuBuffer>(device,list,std::as_bytes(std::span(mesh.vertices)),D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
        gpu.indices=std::make_unique<GpuBuffer>(device,list,std::as_bytes(std::span(mesh.indices)),D3D12_RESOURCE_STATE_INDEX_BUFFER);
        gpu.indexCount=static_cast<UINT>(mesh.indices.size());meshes_.push_back(std::move(gpu));}
    textures_=std::make_unique<TextureManager>(device,list,srv,samplers,scene);
}
void GpuScene::FinishUpload(){for(auto& mesh:meshes_){mesh.vertices->ReleaseUpload();mesh.indices->ReleaseUpload();}textures_->FinishUpload();}
void UploadedMesh::Draw(ID3D12GraphicsCommandList* list) const {
    auto vb=vertices->VertexView(sizeof(Vertex));auto ib=indices->IndexView();list->IASetVertexBuffers(0,1,&vb);list->IASetIndexBuffer(&ib);list->DrawIndexedInstanced(indexCount,1,0,0,0);
}
D3D12_INPUT_LAYOUT_DESC GpuScene::InputLayout(){
    static const D3D12_INPUT_ELEMENT_DESC elements[]={
        {"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,UINT(offsetof(Vertex,position)),D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
        {"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,UINT(offsetof(Vertex,normal)),D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
        {"TANGENT",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,UINT(offsetof(Vertex,tangent)),D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
        {"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,UINT(offsetof(Vertex,texcoord)),D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
        {"TEXCOORD",1,DXGI_FORMAT_R32G32_FLOAT,0,UINT(offsetof(Vertex,texcoord1)),D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
        {"COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,UINT(offsetof(Vertex,color)),D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0}};
    return {elements,6};
}
ObjectConstants GpuScene::Constants(const Entity& entity,const Material& material,FXMMATRIX vp){
    ObjectConstants c{};const auto world=entity.transform.WorldMatrix();bool mirrored=XMVectorGetX(XMMatrixDeterminant(world))<0;
    XMStoreFloat4x4(&c.worldViewProjection,world*vp);XMStoreFloat4x4(&c.world,world);XMStoreFloat4x4(&c.normalMatrix,XMMatrixTranspose(XMMatrixInverse(nullptr,world)));
    c.baseColor=material.baseColor;c.emissiveCutoff={material.emissive.x,material.emissive.y,material.emissive.z,material.alphaCutoff};
    c.factors={material.metallic,material.roughness,material.ao,material.normalScale};c.flags={float(material.alphaMode),float(material.doubleSided),mirrored?-1.0f:1.0f,float(material.unlit)};
    c.extras={material.occlusionStrength,material.textures[size_t(TextureRole::Normal)].texture?1.0f:0.0f,
        material.textures[size_t(TextureRole::OriginalImage)].texture?1.0f:0.0f,0};
    for(size_t i=0;i<MaterialTextureCount;++i){const auto& t=material.textures[i];c.uv[i]={{t.offset.x,t.offset.y,t.scale.x,t.scale.y},{t.rotation,float(t.texCoord),0,0}};}
    return c;
}
}
