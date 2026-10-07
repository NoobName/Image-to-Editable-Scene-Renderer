#pragma once
#include "ScenePackage/RelightingSession.h"
#include "ScenePackage/RelightingReliability.h"
#include "ScenePackage/ScenePackageLoader.h"
namespace isr {
struct RelightingRecipe {
    package::Json document;
    std::shared_ptr<ScenePackage> package;
    std::shared_ptr<const ProtectionMask> importedMask;
    RelightingSession session;
};
const package::Json& RecipeSchema();
package::Json LightingJson(const LightingParameters&);
LightingParameters ParseLightingParameters(const package::Json&);
// Validation is shared by UI save/load and finite-frame CLI. No mutable scene values are serialized.
package::Json RecipeState(const RelightingSession&);
void ApplyRecipeState(RelightingSession&,const package::Json&);
void SaveRecipe(const std::filesystem::path&,const RelightingSession&,const std::shared_ptr<const ProtectionMask>&,bool overwrite=false);
RelightingRecipe LoadRecipe(const std::filesystem::path&);
}
