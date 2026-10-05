#pragma once
#include <DirectXMath.h>
#include <vector>
#include <cstdint>
namespace isr {
struct Vertex {
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT3 normal;
    DirectX::XMFLOAT4 tangent; // xyz direction, w tangent-space handedness.
    DirectX::XMFLOAT2 texcoord;
    DirectX::XMFLOAT2 texcoord1{};
    DirectX::XMFLOAT4 color{1,1,1,1};
};
static_assert(sizeof(Vertex) == 72);
struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    static Mesh Cube();
    static Mesh Sphere(uint32_t slices = 48, uint32_t stacks = 24);
    static Mesh Plane();
};
}
