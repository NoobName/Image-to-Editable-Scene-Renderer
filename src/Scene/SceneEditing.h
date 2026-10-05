#pragma once
#include "Scene/Scene.h"
#include "Scene/Bounds.h"
namespace isr {
// Per-load editor data. No GPU resources or model dependencies.
class SceneEditState {
public:
    bool centerPivot=true;
    void Capture(const Scene&);
    void Reset(){captured_=false;originalMaterials_.clear();originalTransforms_.clear();bounds_.clear();}
    bool Captured()const{return captured_;}
    const Material& OriginalMaterial(size_t index)const{return originalMaterials_.at(index);}
    void RestoreMaterial(Scene&,size_t index)const;
    void RestoreTransform(Scene&,size_t index)const;
    std::optional<Bounds> SelectionBounds(const Scene&,size_t entity)const;
    // Screen coordinates are normalized 0..1 within the rendered image (Y downward).
    std::optional<size_t> Pick(const Scene&,float u,float v)const;
    void EditTransform(Scene&,size_t entity,Transform next,bool preserveCenter)const;
private:
    bool captured_=false;
    std::vector<Material> originalMaterials_;
    std::vector<Transform> originalTransforms_;
    std::vector<Bounds> bounds_;
};
// Direction is light travel, not the vector toward the sun. Drag axes are camera-relative.
DirectX::XMFLOAT3 DragSunDirection(DirectX::XMFLOAT3 direction,const Camera&,float dx,float dy);
}
