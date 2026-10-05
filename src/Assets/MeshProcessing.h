#pragma once
#include "Scene/Mesh.h"
namespace isr {
void GenerateNormals(Mesh& mesh);
void GenerateTangents(Mesh& mesh, unsigned texCoord = 0);
}
