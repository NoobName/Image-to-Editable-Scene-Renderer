#include "Scene/Mesh.h"
#include <cmath>
#include <stdexcept>
namespace isr {
using namespace DirectX;
Mesh Mesh::Plane() {
    // UV +V points toward +Z, whereas cross(+Y,+X) points toward -Z.
    return {{{{-0.5f,0,-0.5f},{0,1,0},{1,0,0,-1},{0,0}},{{0.5f,0,-0.5f},{0,1,0},{1,0,0,-1},{1,0}},
        {{0.5f,0,0.5f},{0,1,0},{1,0,0,-1},{1,1}},{{-0.5f,0,0.5f},{0,1,0},{1,0,0,-1},{0,1}}}, {0,2,1,0,3,2}};
}
Mesh Mesh::Cube() {
    Mesh mesh;
    const XMFLOAT3 normals[] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
    for (const auto& normal : normals) {
        const auto n = XMLoadFloat3(&normal);
        const auto helper = std::abs(normal.y) > 0.5f ? XMVectorSet(0,0,1,0) : XMVectorSet(0,1,0,0);
        const auto u = XMVector3Normalize(XMVector3Cross(helper,n));
        const auto v = XMVector3Cross(n,u);
        const uint32_t base = static_cast<uint32_t>(mesh.vertices.size());
        const float coords[][2] = {{-1,-1},{1,-1},{1,1},{-1,1}};
        XMFLOAT3 tangent; XMStoreFloat3(&tangent,u);
        for (const auto& c : coords) {
            XMFLOAT3 position; XMStoreFloat3(&position,0.5f*(n + c[0]*u + c[1]*v));
            mesh.vertices.push_back({position,normal,{tangent.x,tangent.y,tangent.z,1},{(c[0]+1)*0.5f,(c[1]+1)*0.5f}});
        }
        mesh.indices.insert(mesh.indices.end(),{base,base+1,base+2,base,base+2,base+3});
    }
    return mesh;
}
Mesh Mesh::Sphere(uint32_t slices, uint32_t stacks) {
    if (slices < 3 || stacks < 2 || slices > 1024 || stacks > 1024) throw std::invalid_argument("Invalid sphere subdivisions");
    Mesh mesh;
    for (uint32_t y = 0; y <= stacks; ++y) {
        const float v = static_cast<float>(y)/static_cast<float>(stacks), phi = v*XM_PI;
        for (uint32_t x = 0; x <= slices; ++x) {
            const float u = static_cast<float>(x)/static_cast<float>(slices), theta = u*XM_2PI;
            const XMFLOAT3 n{std::sin(phi)*std::cos(theta),std::cos(phi),std::sin(phi)*std::sin(theta)};
            mesh.vertices.push_back({n,n,{-std::sin(theta),0,std::cos(theta),1},{u,v}});
        }
    }
    for (uint32_t y = 0; y < stacks; ++y) for (uint32_t x = 0; x < slices; ++x) {
        const uint32_t a = y*(slices+1)+x, b = a+slices+1;
        // Skip degenerate pole triangles. Cross products point outward.
        if (y != 0) mesh.indices.insert(mesh.indices.end(),{a,a+1,b});
        if (y != stacks-1) mesh.indices.insert(mesh.indices.end(),{a+1,b+1,b});
    }
    return mesh;
}
}
