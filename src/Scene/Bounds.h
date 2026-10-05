#pragma once
#include "Scene/Scene.h"
namespace isr {
struct Bounds { DirectX::XMFLOAT3 minimum,maximum; };
Bounds MeshBounds(const Mesh&);
Bounds WorldBounds(const Scene&,const std::vector<Bounds>&);
DirectX::XMMATRIX FitDirectionalShadow(const Bounds&,DirectX::XMFLOAT3 direction,unsigned resolution);
}
