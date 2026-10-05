#pragma once
#include <DirectXMath.h>
#include <optional>
namespace isr {
struct Transform {
    DirectX::XMFLOAT3 position{0,0,0};
    DirectX::XMFLOAT3 rotation{0,0,0}; // Pitch, yaw, roll in radians.
    DirectX::XMFLOAT3 scale{1,1,1};
    std::optional<DirectX::XMFLOAT4X4> importedLocal; // Preserves glTF quaternion/matrix transforms, including reflection.
    DirectX::XMMATRIX LocalMatrix() const;
    // Convert an imported SRT matrix without losing reflection. Shear stays as
    // a matrix base; editable TRS then acts as an offset before that base.
    bool TryDecomposeImported();
    DirectX::XMMATRIX WorldMatrix() const { return DirectX::XMLoadFloat4x4(&world_); }
    void UpdateWorld(DirectX::FXMMATRIX parent = DirectX::XMMatrixIdentity());
private:
    DirectX::XMFLOAT4X4 world_{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
};
}
