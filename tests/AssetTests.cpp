#include "Assets/AssetManager.h"
#include "Assets/TextureProcessing.h"
#include "Assets/GltfSupport.h"
#include "Assets/MeshProcessing.h"
#include <cmath>
#include <iostream>
using namespace isr;
using namespace DirectX;
namespace {
int checks=0;
void Require(bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);}
bool Near(float a,float b){return std::abs(a-b)<1e-4f;}
void Accessors(){
    // A sparse override in a strided position buffer, and normalized byte colors.
    float positions[]={0,0,0,99,1,0,0,99,0,1,0,99}, replacement[]={0,2,0};uint8_t sparseIndex=2;
    cgltf_buffer buffers[3]{};buffers[0].data=positions;buffers[1].data=&sparseIndex;buffers[2].data=replacement;
    cgltf_buffer_view views[3]{};for(int i=0;i<3;++i)views[i].buffer=&buffers[i];
    cgltf_accessor a{};a.buffer_view=&views[0];a.count=3;a.type=cgltf_type_vec3;a.component_type=cgltf_component_type_r_32f;a.stride=16;
    a.is_sparse=true;a.sparse.count=1;a.sparse.indices_buffer_view=&views[1];a.sparse.indices_component_type=cgltf_component_type_r_8u;a.sparse.values_buffer_view=&views[2];
    auto p=gltf::Floats(&a,3);Require(p.size()==9&&Near(p[7],2)&&Near(p[3],1),"Sparse/strided positions failed");
    uint8_t rgba[]={255,128,0,255};buffers[0].data=rgba;a={};a.buffer_view=&views[0];a.count=1;a.type=cgltf_type_vec4;a.component_type=cgltf_component_type_r_8u;a.stride=4;a.normalized=true;
    auto color=gltf::Floats(&a,4);Require(Near(color[0],1)&&Near(color[1],128.0f/255),"Normalized accessor failed");
    uint16_t indices[]={0,1,2};buffers[0].data=indices;a={};a.buffer_view=&views[0];a.count=3;a.type=cgltf_type_scalar;a.component_type=cgltf_component_type_r_16u;a.stride=2;
    Require(gltf::Indices(&a,3)==std::vector<uint32_t>({0,1,2}),"16-bit index decode failed");
    Require(gltf::Indices(nullptr,3)==std::vector<uint32_t>({0,1,2}),"Nonindexed primitive failed");
    indices[2]=9;bool rejected=false;try{gltf::Indices(&a,3);}catch(...){rejected=true;}Require(rejected,"Invalid index accepted");
}
}
int main(){try{
    AssetManager assets;const std::filesystem::path root=ASSET_FIXTURE_DIR;
    auto scene=assets.LoadModel(root/"MaterialLab.gltf"),again=assets.LoadModel(root/"MaterialLab.gltf"),glb=assets.LoadModel(root/"MaterialLab.glb");
    bool quoteHint=false;
    try{assets.LoadModel(std::filesystem::path(L"'"+(root/"MaterialLab.gltf").native()+L"'"));}
    catch(const std::exception& e){const std::string message=e.what();quoteHint=message.find("CMD treats single quotes")!=std::string::npos&&message.find("Use double quotes")!=std::string::npos;}
    Require(quoteHint,"CMD literal-quote path error has no actionable hint");
    Require(assets.ModelCount()==2,"Canonical model cache failed");
    Require(scene.meshes.size()==3&&scene.textures.size()==4&&scene.materials.size()==4,"glTF counts failed");
    Require(scene.entities.size()==glb.entities.size(),"GLB hierarchy differs");
    Require(scene.textures[0].image==again.textures[0].image,"Repeated load decoded image again");
    for(size_t i=0;i<scene.textures.size();++i)Require(scene.textures[i].image->rgba==glb.textures[i].image->rgba,"GLB embedded image differs");
    auto& material=scene.materials[0];
    Require(material.textures[1].texture==material.textures[4].texture,"Packed ORM not shared");
    Require(scene.materials[0].textures[0].texture==scene.materials[1].textures[0].texture,"Shared base texture lost");
    Require(IsSrgb(TextureRole::BaseColor)&&IsSrgb(TextureRole::Emissive)&&!IsSrgb(TextureRole::Normal)&&!IsSrgb(TextureRole::Occlusion)&&!IsSrgb(TextureRole::MetallicRoughness),"Color role mismatch");
    material.metallic=.123f;Require(!Near(again.materials[0].metallic,.123f),"Editable material aliased cache");
    bool mirrored=false;size_t drawables=0;
    for(const auto& e:scene.entities)if(e.renderer){++drawables;mirrored|=XMVectorGetX(XMMatrixDeterminant(e.transform.WorldMatrix()))<0;}
    Require(mirrored&&drawables==4,"Node hierarchy/instancing/mirror lost");
    for(size_t i=0;i<scene.meshes.size();++i){const auto& mesh=scene.meshes[i];Require(mesh.indices==glb.meshes[i].indices,"GLB geometry differs");
        for(const auto& v:mesh.vertices){auto n=XMLoadFloat3(&v.normal),t=XMLoadFloat4(&v.tangent);
            Require(Near(XMVectorGetX(XMVector3Length(t)),1),"Generated tangent not unit");
            Require(Near(XMVectorGetX(XMVector3Dot(n,t)),0),"Generated tangent not orthogonal");
            Require(Near(std::abs(v.tangent.w),1),"Invalid tangent sign");}}
    ImageData image;image.width=2;image.height=1;image.rgba={0,0,0,0,255,255,255,255};
    const auto srgb=BuildMipChain(image,true),linear=BuildMipChain(image,false);
    Require(srgb.back().rgba[0]==188&&linear.back().rgba[0]==128,"Mips averaged sRGB encoded values");
    Require(srgb.back().rgba[3]==128,"Alpha incorrectly gamma transformed");
    image.width=3;image.rgba={0,0,0,255,0,0,0,255,255,255,255,255};Require(BuildMipChain(image,false).back().rgba[0]==85,"Odd mip dropped edge texel");
    auto flat=Mesh::Cube();GenerateNormals(flat);Require(flat.vertices.size()==flat.indices.size(),"Missing normals must split corners");
    for(size_t i=0;i<flat.indices.size();i+=3){auto a=XMLoadFloat3(&flat.vertices[i].normal),b=XMLoadFloat3(&flat.vertices[i+1].normal);
        Require(Near(XMVectorGetX(XMVector3Length(a)),1)&&Near(XMVectorGetX(XMVector3Dot(a,b)),1),"Flat normal generation failed");}
    Accessors();std::cout<<checks<<" asset checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
