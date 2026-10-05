#include "Assets/GltfSupport.h"
#include "Assets/MeshProcessing.h"
#include <stdexcept>
namespace isr::gltf {
Mesh Primitive(const cgltf_primitive& primitive,const Material& material) {
    if (primitive.has_draco_mesh_compression || primitive.targets_count) throw std::runtime_error("Draco and morph targets are not implemented");
    const cgltf_accessor *position=nullptr,*normal=nullptr,*tangent=nullptr,*uv[2]{},*color=nullptr;
    for (size_t i=0;i<primitive.attributes_count;++i) {
        const auto& a=primitive.attributes[i];
        if (a.type==cgltf_attribute_type_position) position=a.data;
        if (a.type==cgltf_attribute_type_normal) normal=a.data;
        if (a.type==cgltf_attribute_type_tangent) tangent=a.data;
        if (a.type==cgltf_attribute_type_texcoord && a.index>=0 && a.index<2) uv[a.index]=a.data;
        if (a.type==cgltf_attribute_type_color && a.index==0) color=a.data;
        if (a.type==cgltf_attribute_type_joints || a.type==cgltf_attribute_type_weights) throw std::runtime_error("Skinning is not implemented");
    }
    auto positions=Floats(position,3); Mesh mesh; mesh.vertices.resize(position->count);
    for (size_t i=0;i<mesh.vertices.size();++i) mesh.vertices[i].position={positions[i*3],positions[i*3+1],-positions[i*3+2]};
    auto assign=[&](const cgltf_accessor* a,size_t count,auto setter) {
        if (!a) return; if (a->count!=position->count) throw std::runtime_error("Attribute counts differ");
        auto values=Floats(a,count); for (size_t i=0;i<mesh.vertices.size();++i) setter(mesh.vertices[i],values.data()+i*count);
    };
    assign(normal,3,[](Vertex& v,const float* x){v.normal={x[0],x[1],-x[2]};});
    assign(tangent,4,[](Vertex& v,const float* x){v.tangent={x[0],x[1],-x[2],-x[3]};});
    assign(uv[0],2,[](Vertex& v,const float* x){v.texcoord={x[0],x[1]};});
    assign(uv[1],2,[](Vertex& v,const float* x){v.texcoord1={x[0],x[1]};});
    if (color) { const auto n=cgltf_num_components(color->type); if(n!=3&&n!=4)throw std::runtime_error("Invalid COLOR_0");
        assign(color,n,[n](Vertex& v,const float* x){v.color={x[0],x[1],x[2],n==4?x[3]:1};}); }
    for (const auto& slot:material.textures) if (slot.texture && !uv[slot.texCoord]) throw std::runtime_error("Material references missing UV set");
    const auto input=Indices(primitive.indices,mesh.vertices.size());
    auto triangle=[&](uint32_t a,uint32_t b,uint32_t c){ if(a!=b&&a!=c&&b!=c)mesh.indices.insert(mesh.indices.end(),{a,c,b}); };
    if (primitive.type==cgltf_primitive_type_triangles) {
        if (input.size()%3) throw std::runtime_error("Triangle indices are not divisible by three");
        for(size_t i=0;i<input.size();i+=3)triangle(input[i],input[i+1],input[i+2]);
    } else if (primitive.type==cgltf_primitive_type_triangle_strip) {
        for(size_t i=2;i<input.size();++i)if(i%2)triangle(input[i-1],input[i-2],input[i]);else triangle(input[i-2],input[i-1],input[i]);
    } else if (primitive.type==cgltf_primitive_type_triangle_fan) {
        for(size_t i=2;i<input.size();++i)triangle(input[0],input[i-1],input[i]);
    } else throw std::runtime_error("Only triangle, strip and fan primitives are supported");
    if(mesh.indices.empty())throw std::runtime_error("Empty glTF primitive");
    if(!normal)GenerateNormals(mesh);
    if(!tangent)GenerateTangents(mesh,material.textures[size_t(TextureRole::Normal)].texCoord);
    return mesh;
}
}
