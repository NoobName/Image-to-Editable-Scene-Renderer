#pragma once
#include "Scene/Scene.h"
#include "ScenePackage/SourceObservation.h"
namespace isr {
// A camera-space observation shell. Never borrowed from the editable Scene.
struct SourceGeometry {
    Scene scene;
    NumericImage evidence; // observed V, old support, receiver support, covered receiver
    package::Json report;
    bool available=false;
};
SourceGeometry BuildSourceGeometry(const SourceObservation&);
}
