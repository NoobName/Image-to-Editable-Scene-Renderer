#include "Scene/Transform.h"
#include <stdexcept>
#include <cmath>
#include <algorithm>
namespace isr {
using namespace DirectX;
XMMATRIX Transform::LocalMatrix() const {
    if (!(std::abs(scale.x)>0.0001f && std::abs(scale.y)>0.0001f && std::abs(scale.z)>0.0001f) ||
        !std::isfinite(scale.x + scale.y + scale.z + position.x + position.y + position.z + rotation.x + rotation.y + rotation.z))
        throw std::invalid_argument("Transform needs finite components and nonsingular scale");
    const auto trs=XMMatrixScaling(scale.x,scale.y,scale.z)*XMMatrixRotationRollPitchYaw(rotation.x,rotation.y,rotation.z)
        *XMMatrixTranslation(position.x,position.y,position.z);
    if (importedLocal) {
        const auto matrix = XMLoadFloat4x4(&*importedLocal);
        for (const auto& row : importedLocal->m) for (float value : row)
            if (!std::isfinite(value)) throw std::invalid_argument("Nonfinite node transform");
        if (std::abs(XMVectorGetX(XMMatrixDeterminant(matrix))) < 1e-12f)
            throw std::invalid_argument("Singular glTF node transform");
        return trs*matrix;
    }
    return trs;
}
bool Transform::TryDecomposeImported() {
    if(!importedLocal)return true;
    XMVECTOR s,q,t;const auto original=LocalMatrix();
    if(!XMMatrixDecompose(&s,&q,&t,original))return false;
    XMFLOAT4X4 r;XMStoreFloat4x4(&r,XMMatrixRotationQuaternion(q));
    Transform candidate;XMStoreFloat3(&candidate.scale,s);XMStoreFloat3(&candidate.position,t);
    if(std::abs(candidate.scale.x)<=0.0001f||std::abs(candidate.scale.y)<=0.0001f||std::abs(candidate.scale.z)<=0.0001f)return false;
    const float cosPitch=std::hypot(r._31,r._33);
    candidate.rotation.x=std::atan2(-r._32,cosPitch);
    if(cosPitch>0.00001f){
        candidate.rotation.y=std::atan2(r._31,r._33);candidate.rotation.z=std::atan2(r._12,r._22);
    }else candidate.rotation.y=std::atan2(-r._13,r._11);
    XMFLOAT4X4 before,after;XMStoreFloat4x4(&before,original);XMStoreFloat4x4(&after,candidate.LocalMatrix());
    for(int row=0;row<4;++row)for(int col=0;col<4;++col)
        if(std::abs(before.m[row][col]-after.m[row][col])>0.0002f*std::max(1.0f,std::abs(before.m[row][col])))return false;
    position=candidate.position;rotation=candidate.rotation;scale=candidate.scale;importedLocal.reset();return true;
}
void Transform::UpdateWorld(FXMMATRIX parent) { XMStoreFloat4x4(&world_, LocalMatrix() * parent); }
}
