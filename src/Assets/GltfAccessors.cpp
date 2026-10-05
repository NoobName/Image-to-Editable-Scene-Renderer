#include "Assets/GltfSupport.h"
#include <cmath>
#include <cstring>
#include <numeric>
#include <stdexcept>
namespace isr::gltf {
void CheckResult(cgltf_result result,const char* operation) {
    if (result!=cgltf_result_success) throw std::runtime_error(std::string(operation)+" failed (cgltf result "+std::to_string(result)+")");
}
std::vector<float> Floats(const cgltf_accessor* accessor,size_t components) {
    if (!accessor || cgltf_num_components(accessor->type)!=components || accessor->count>10'000'000)
        throw std::runtime_error("Missing, oversized or incorrectly shaped glTF attribute");
    std::vector<float> values(accessor->count*components);
    if (cgltf_accessor_unpack_floats(accessor,values.data(),values.size())!=values.size()) throw std::runtime_error("Cannot unpack glTF attribute");
    for (auto v:values) if (!std::isfinite(v)) throw std::runtime_error("Nonfinite vertex attribute");
    return values;
}
static uint32_t ReadUnsigned(const uint8_t* data,cgltf_component_type type) {
    if (type==cgltf_component_type_r_8u) return *data;
    if (type==cgltf_component_type_r_16u) { uint16_t v; std::memcpy(&v,data,2); return v; }
    if (type==cgltf_component_type_r_32u) { uint32_t v; std::memcpy(&v,data,4); return v; }
    throw std::runtime_error("Indices must use unsigned byte/short/int");
}
std::vector<uint32_t> Indices(const cgltf_accessor* accessor,size_t vertexCount) {
    if (!accessor) { std::vector<uint32_t> out(vertexCount); std::iota(out.begin(),out.end(),0u); return out; }
    if (accessor->type!=cgltf_type_scalar || accessor->count>30'000'000) throw std::runtime_error("Invalid index accessor");
    if(accessor->component_type!=cgltf_component_type_r_8u&&accessor->component_type!=cgltf_component_type_r_16u&&accessor->component_type!=cgltf_component_type_r_32u)
        throw std::runtime_error("Index component must be unsigned integer");
    std::vector<uint32_t> out(accessor->count,0);
    if (accessor->buffer_view) {
        auto base=*accessor; base.is_sparse=false;
        if (cgltf_accessor_unpack_indices(&base,out.data(),sizeof(uint32_t),out.size())!=out.size()) throw std::runtime_error("Cannot unpack indices");
    }
    if (accessor->is_sparse) {
        const auto& sparse=accessor->sparse;
        const auto* indices=cgltf_buffer_view_data(sparse.indices_buffer_view)+sparse.indices_byte_offset;
        const auto* values=cgltf_buffer_view_data(sparse.values_buffer_view)+sparse.values_byte_offset;
        for (size_t i=0;i<sparse.count;++i) {
            auto index=ReadUnsigned(indices+i*cgltf_component_size(sparse.indices_component_type),sparse.indices_component_type);
            if (index>=out.size()) throw std::runtime_error("Sparse index overwrite outside accessor");
            out[index]=ReadUnsigned(values+i*cgltf_component_size(accessor->component_type),accessor->component_type);
        }
    }
    for (auto value:out) if (value>=vertexCount) throw std::runtime_error("Index outside vertex buffer");
    return out;
}
}
