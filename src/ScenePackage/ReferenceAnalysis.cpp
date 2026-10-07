#include "ScenePackage/ReferenceAnalysis.h"
#include "ScenePackage/RelightingRecipe.h"
#include "ScenePackage/AnchorImage.h"
#include "EmbeddedSchema.h"
#include "ScenePackage/PackageManifest.h"
#include "Assets/AssetIO.h"
#include <cmath>
namespace isr {
std::shared_ptr<const ReferenceAnalysis> LoadReferenceAnalysis(const std::filesystem::path& input){
    const auto path=std::filesystem::is_directory(input)?input/"reference.json":input;auto result=std::make_shared<ReferenceAnalysis>();
    static const auto schema=package::ParseSchema(package::ReferenceSchemaText);result->metadata=package::ReadJson(path);const auto& j=result->metadata;
    package::Validate(j,schema,"reference",schema);result->root=std::filesystem::canonical(path.parent_path());
    result->target=ParseLightingParameters(j.at("proposal").at("target"));result->sourceBaseline=ParseLightingParameters(j.at("source").at("baseline"));
    result->confidence=j.at("proposal").at("confidence");result->canApply=j.at("proposal").at("canApply");
    if(result->canApply&&(result->confidence<.0375f||!j.at("proposal").at("directionSupported").get<bool>()||!j.at("fit").at("identifiable").get<bool>()||j.at("fit").at("albedoSource")!="intrinsic"))throw std::runtime_error("Reference proposal claims unsupported illumination");
    if(!result->canApply&&(result->target!=result->sourceBaseline||result->confidence!=0))throw std::runtime_error("Unavailable reference must retain baseline and zero confidence");
    size_t i=0;for(const char* key:{"reference","residual"}){
        const auto& image=j.at("images").at(key);const auto size=image.at("size").get<std::array<uint32_t,2>>();
        if(size[0]>2048||size[1]>2048)throw std::runtime_error("Reference preview exceeds 2048 edge limit");
        package::ValidateAnchorImage(result->root,image,false,&result->previews[i++]);}
    if(std::filesystem::exists(result->root/"optimization.json")){
        static const auto optimizationSchema=package::ParseSchema(package::OptimizationSchemaText);
        result->optimization=package::ReadJson(ConstrainAssetPath(result->root/"optimization.json",result->root));const auto& o=result->optimization;
        package::Validate(o,optimizationSchema,"optimization",optimizationSchema);ParseLightingParameters(o.at("initialEffectiveTarget"));
        if(o.at("relation")!=j.at("proposal").at("relation"))throw std::runtime_error("Optimization reference relation mismatch");
        if((o.at("status")=="improved")!=result->canApply)throw std::runtime_error("Optimization status/proposal mismatch");
        if(result->canApply&&(!(o.at("bestLoss").get<double>()<o.at("initialLoss").get<double>())||o.at("samples").get<unsigned>()<64||o.at("history").empty()))throw std::runtime_error("Optimization has no supported improvement");
        if(result->canApply&&o.at("relation")=="same-scene"&&!o.at("registered").get<bool>())throw std::runtime_error("Same-scene optimization requires registration declaration");
        double previous=o.at("initialLoss");for(const auto& h:o.at("history")){const double loss=h.at("bestLoss");if(loss>previous+1e-12)throw std::runtime_error("Optimization best loss increased");previous=loss;}
        const auto& c=o.at("candidate");const auto candidatePath=package::AssetPath(result->root,c.at("path"),"debug",{".dds"});
        if(package::Sha256(ReadAssetFile(candidatePath))!=c.at("sha256").get<std::string>())throw std::runtime_error("Optimization candidate hash mismatch");
        result->candidate=LoadNumericDds(candidatePath);const auto& image=*result->candidate;
        if(image.format!=NumericFormat::Vector||c.at("size")!=package::Json::array({image.width,image.height}))throw std::runtime_error("Optimization candidate dimensions/format mismatch");
    }
    return result;
}
void CheckReferenceSource(const ReferenceAnalysis& reference,const RelightingSession& session){
    if(!session.CanDisplayImage())throw std::runtime_error("Reference matching requires a fixed source anchor");
    const auto& j=reference.metadata.at("source");const auto& anchor=session.Source()->anchor->metadata;
    if(j.at("sourceId")!=anchor.at("sourceId")||j.at("anchorSha256")!=anchor.at("sourceImage").at("sha256"))throw std::runtime_error("Reference proposal belongs to another source image");
    if(reference.sourceBaseline!=session.lighting.source||j.at("baselineRevision").get<uint64_t>()!=session.lighting.sourceRevision)
        throw std::runtime_error("Reference source calibration changed; analyze again before applying");
    if(!reference.optimization.is_null()){
        auto checked=session;ApplyRecipeState(checked,reference.optimization.at("initialState"));
        // Compare the captured user state, not just scene revision: slider edits do not load a new scene.
        if(RecipeState(session)!=reference.optimization.at("initialState"))throw std::runtime_error("Optimization stale: target, protection or display changed during the task");
        const auto initial=ParseLightingParameters(reference.optimization.at("initialEffectiveTarget"));
        const auto effective=session.lighting.EffectiveTarget();
        if(initial.direction!=effective.direction||initial.directColor!=effective.directColor||initial.ambientColor!=effective.ambientColor||
            std::abs(initial.directIntensity-effective.directIntensity)>1e-5f||std::abs(initial.ambientIntensity-effective.ambientIntensity)>1e-5f)
            throw std::runtime_error("Optimization initial effective target disagrees with captured state");
        double dot=0;for(size_t i=0;i<3;++i)dot+=double(initial.direction[i])*reference.target.direction[i];
        if(reference.canApply&&dot<-1e-5)throw std::runtime_error("Optimization exceeds 90 degree direction bound");
        auto energy=[](const LightingParameters& p){constexpr double y[]{.2126,.7152,.0722};double value=0;for(size_t i=0;i<3;++i)value+=y[i]*(double(p.directColor[i])*p.directIntensity+double(p.ambientColor[i])*p.ambientIntensity);return value;};
        if(reference.canApply&&std::abs(energy(reference.target)-energy(session.lighting.source))>1e-5*std::max(1.,energy(session.lighting.source)))throw std::runtime_error("Optimization violates fixed intensity/exposure gauge");
    }
}
bool ApplyReferenceProposal(RelightingSession& session){
    auto& reference=session.reference;if(!reference.analysis)return false;if(reference.applied)return true;CheckReferenceSource(*reference.analysis,session);if(!reference.analysis->canApply)return false;
    if(!reference.analysis->optimization.is_null()&&!reference.gpuVerified)throw std::runtime_error("Optimization requires successful DX12 candidate verification");
    // Validate through the same recipe/state contract, but copy only target fields so display
    // mapping, protection and the immutable source baseline are not reset by applying a proposal.
    auto state=RecipeState(session);state["target"]=LightingJson(reference.analysis->target);state["targetGlobalGain"]=1;
    auto checked=session;ApplyRecipeState(checked,state);
    if(!reference.applied){reference.previousTarget=session.lighting.target;reference.previousGain=session.lighting.targetGlobalGain;reference.previousExposure=session.display.exposure;}
    session.lighting.target=checked.lighting.target;session.lighting.targetGlobalGain=1;reference.applied=true;return true;
}
bool ResetReferenceTarget(RelightingSession& session){
    auto& reference=session.reference;if(!reference.analysis||!reference.applied)return false;
    auto checked=session;checked.lighting.target=reference.previousTarget;checked.lighting.targetGlobalGain=reference.previousGain;checked.display.exposure=reference.previousExposure;CheckReferenceSource(*reference.analysis,checked);
    session.lighting.target=reference.previousTarget;session.lighting.targetGlobalGain=reference.previousGain;session.display.exposure=reference.previousExposure;reference.applied=false;return true;
}
}
