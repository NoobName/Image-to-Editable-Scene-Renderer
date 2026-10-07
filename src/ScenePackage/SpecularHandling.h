#pragma once
#include "ScenePackage/SourceObservation.h"
namespace isr {
struct SpecularEvidence {
    NumericImage candidate; // RGB: bounded supported original component; A: behavior confidence.
    NumericImage protectedResidual; // RGB: signed unexplained residual; A: candidate clipping error.
    package::Json metadata;
    float scale=1;
    bool available=false;
};
std::array<float,3> EvaluateImageGgx(const std::array<float,3>& normal,const std::array<float,3>& position,
    const std::array<float,3>& albedo,float roughness,float metallic,const LightingParameters&);
SpecularEvidence BuildSpecularEvidence(const SourceObservation&);
}
