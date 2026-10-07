#include "ScenePackage/RelightingRecipe.h"
#include "ScenePackage/AnchorImage.h"
#include "Assets/AssetIO.h"
#include "Core/AtomicFile.h"
#include "EmbeddedSchema.h"
#include <set>
namespace isr {
namespace {
using package::Json;
void Require(bool value,const char* message){if(!value)throw std::runtime_error(std::string("Relighting recipe: ")+message);}
Json RecipeLight(const LightingParameters& p){return {{"direction",p.direction},{"directColor",p.directColor},{"ambientColor",p.ambientColor},{"directIntensity",p.directIntensity},{"ambientIntensity",p.ambientIntensity}};}
LightingParameters RecipeLight(const Json& j){LightingParameters p;p.direction=j.at("direction").get<std::array<float,3>>();p.directColor=j.at("directColor").get<std::array<float,3>>();p.ambientColor=j.at("ambientColor").get<std::array<float,3>>();p.directIntensity=j.at("directIntensity");p.ambientIntensity=j.at("ambientIntensity");
    double length=0;for(float v:p.direction)length+=double(v)*v;Require(std::abs(length-1)<1e-4,"light travel direction must have unit length");return p;}
std::filesystem::path Relative(const std::string& text){
    Require(!text.empty()&&text.size()<32768&&text.find_first_of(":\\\0",0,3)==std::string::npos,"expected portable relative path with forward slashes");
    const auto path=std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(text.data()),text.size()));
    Require(!path.is_absolute()&&!path.has_root_path(),"absolute path is unsupported");return path;
}
Json Identity(const SourceObservation& s){
    Require(s.CanDisplayImage(),"source anchor unavailable");Json manifests=Json::object();
    // Sidecars contain all analysis/backend revisions and dependency hashes. Existing loaders
    // additionally validate the referenced numeric assets, so a stale sidecar cannot hide changed maps.
    for(const char* name:{"scene.json","relighting/relighting.json","analysis/analysis.json","lighting/lighting.json","intrinsic/intrinsic.json","shadow/shadow.json"}){
        const auto path=s.packageRoot/name;if(std::filesystem::exists(path))manifests[name]=package::Sha256(ReadAssetFile(ConstrainAssetPath(path,s.packageRoot)));
    }
    return {{"sourceId",s.anchor->metadata.at("sourceId")},{"anchorSha256",s.anchor->metadata.at("sourceImage").at("sha256")},{"manifests",manifests},
        {"analysisRevision",s.analysisMaps?s.analysisMaps->metadata:Json(nullptr)},{"lightingRevision",s.lighting?s.lighting->metadata.at("fit"):Json(nullptr)}};
}
void OutsidePackage(const std::filesystem::path& file,const std::filesystem::path& root){
    const auto destination=std::filesystem::weakly_canonical(file),base=std::filesystem::canonical(root);auto d=destination.begin();bool inside=true;
    for(auto r=base.begin();r!=base.end();++r,++d)if(d==destination.end()||_wcsicmp(d->c_str(),r->c_str())!=0){inside=false;break;}
    Require(!inside,"save outside the source package; source package is immutable");
}
}
const package::Json& RecipeSchema(){static const auto schema=package::ParseSchema(package::RecipeSchemaText);return schema;}
package::Json LightingJson(const LightingParameters& p){return RecipeLight(p);}
LightingParameters ParseLightingParameters(const package::Json& j){const auto& schema=RecipeSchema();package::Validate(j,schema.at("$defs").at("light"),"lighting",schema);return RecipeLight(j);}
package::Json RecipeState(const RelightingSession& s){
    Json response;const auto& p=s.relighting;
#define FIELD(n) response[#n]=p.n
    FIELD(strength);FIELD(epsilon);FIELD(minRatio);FIELD(maxRatio);FIELD(stability);FIELD(intrinsicProtection);FIELD(specularEnabled);FIELD(castShadows);
    FIELD(shadowStrength);FIELD(shadowConfidence);FIELD(shadowMaxDelta);FIELD(shadowBias);FIELD(shadowNormalBias);FIELD(shadowPcfRadius);
    FIELD(specularStrength);FIELD(specularMaxDelta);FIELD(specularRoughnessScale);FIELD(chromaLimit);FIELD(relativeDepthEdge);FIELD(normalCosineEdge);
#undef FIELD
    response["colorMode"]=int(p.colorMode);Json regions=Json::array();
    for(const auto& [label,weight]:s.protection.weights){
        const auto found=std::find_if(s.Source()->regions.begin(),s.Source()->regions.end(),[&](const auto& r){return r.label==label;});
        Require(found!=s.Source()->regions.end(),"protection label has no stable region ID");regions.push_back({{"id",found->id},{"label",label},{"weight",weight}});
    }
    return {{"sourceCalibration",{{"light",RecipeLight(s.lighting.source)},{"revision",s.lighting.sourceRevision},{"manual",s.lighting.manualSource},{"fitCacheValid",s.lighting.cacheValid}}},
        {"target",RecipeLight(s.lighting.target)},{"targetGlobalGain",s.lighting.targetGlobalGain},{"response",response},{"protectedRegions",regions},
        {"displayExposure",s.display.exposure},{"confidenceOverlay",s.display.lowConfidence},{"confidenceThreshold",s.display.confidenceThreshold},
        {"imageFog",{{"version",1},{"sourceAdditionalDensity",0},{"enabled",s.fog.enabled},{"density",s.fog.density},{"airlightLinear",s.fog.airlight},{"allowRelativeScale",s.fog.allowRelativeScale}}}};
}
void ApplyRecipeState(RelightingSession& s,const package::Json& j){
    const auto& schema=RecipeSchema();package::Validate(j,schema.at("$defs").at("state"),"recipe.state",schema);
    Require(s.CanDisplayImage(),"source anchor unavailable");auto next=s;
    // Old recipes predate additional atmosphere. Missing fog means OFF, not inherited UI memory.
    next.fog={};if(j.contains("imageFog")){const auto& fog=j.at("imageFog");next.fog.enabled=fog.at("enabled");next.fog.density=fog.at("density");
        next.fog.airlight=fog.at("airlightLinear").get<std::array<float,3>>();next.fog.allowRelativeScale=fog.at("allowRelativeScale");}
    Require(next.fog.Valid(),"additional fog parameters outside supported bounds");
    const auto& calibration=j.at("sourceCalibration");auto& l=next.lighting;
    l.source=RecipeLight(calibration.at("light"));l.draft=l.source;l.sourceRevision=calibration.at("revision");l.manualSource=calibration.at("manual");l.cacheValid=calibration.at("fitCacheValid");
    Require(!l.manualSource||(!l.cacheValid&&l.sourceRevision>0),"manual calibration requires a new revision and invalidated fit cache");
    if(!l.manualSource)Require(l.source==s.lighting.source&&l.sourceRevision==0&&l.cacheValid==s.lighting.cacheValid,"unmodified baseline does not match package");
    l.target=RecipeLight(j.at("target"));l.targetGlobalGain=j.at("targetGlobalGain");auto& p=next.relighting;const auto& response=j.at("response");
#define FIELD(n) response.at(#n).get_to(p.n)
    FIELD(strength);FIELD(epsilon);FIELD(minRatio);FIELD(maxRatio);FIELD(stability);FIELD(intrinsicProtection);FIELD(specularEnabled);FIELD(castShadows);
    FIELD(shadowStrength);FIELD(shadowConfidence);FIELD(shadowMaxDelta);FIELD(shadowBias);FIELD(shadowNormalBias);FIELD(shadowPcfRadius);
    FIELD(specularStrength);FIELD(specularMaxDelta);FIELD(specularRoughnessScale);FIELD(chromaLimit);FIELD(relativeDepthEdge);FIELD(normalCosineEdge);
#undef FIELD
    p.colorMode=RatioColorMode(response.at("colorMode").get<int>());Require(p.Valid(),"material response outside supported bounds");
    next.protection={};std::set<uint32_t> seen;
    for(const auto& r:j.at("protectedRegions")){const auto label=r.at("label").get<uint32_t>();
        Require(seen.insert(label).second,"duplicate region label");
        Require(std::any_of(s.Source()->regions.begin(),s.Source()->regions.end(),[&](const auto& region){return region.label==label&&region.id==r.at("id").get<std::string>();}),"stable region ID/label mismatch");
        next.protection.Apply(label,r.at("weight"));}
    next.display={};next.display.exposure=j.at("displayExposure");next.display.lowConfidence=j.at("confidenceOverlay");next.display.confidenceThreshold=j.at("confidenceThreshold");
    next.SetMode(WorkMode::ImageRelighting);next.imageView=ImageDebugView::Relighted;s=std::move(next);
}
void SaveRecipe(const std::filesystem::path& input,const RelightingSession& s,const std::shared_ptr<const ProtectionMask>& mask,bool overwrite){
    Require(s.CanDisplayImage(),"source anchor unavailable");const auto path=std::filesystem::absolute(input);OutsidePackage(path,s.Source()->packageRoot);
    Require(path.extension()==L".json","recipe extension must be .json");
    const auto state=RecipeState(s);RelightingSession check;check.Publish(s.Source());ApplyRecipeState(check,state);
    const auto identity=Identity(*s.Source());const auto verified=ScenePackageLoader{}.Load(s.Source()->packageRoot);
    Require(verified.observation&&Identity(*verified.observation)==identity,"package changed since this session was loaded");
    Json j={{"version",2},{"rendererRevision","image-relighting-33-v2"},{"parameterContract","bounded-response-fog-v2"},
        {"sourcePackage",PathUtf8(std::filesystem::relative(s.Source()->packageRoot,path.parent_path()).generic_wstring())},
        {"identity",identity},{"state",state},{"importedMask",nullptr}};
    Relative(j["sourcePackage"].get<std::string>());
    // Content-addressed mask assets never replace earlier files; publication is the recipe rename.
    if(mask){const auto original=std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(mask->metadata.at("path").get_ref<const std::string&>().c_str())));
        const auto bytes=ReadAssetFile(original);const auto hash=package::Sha256(bytes);Require(hash==mask->metadata.at("sha256").get<std::string>(),"imported mask changed since loading");
        const auto directory=path.parent_path()/"recipe-assets";std::filesystem::create_directories(directory);const auto target=directory/(hash+".png");
        if(!std::filesystem::exists(target))AtomicWrite(target,bytes);else Require(package::Sha256(ReadAssetFile(target))==hash,"mask asset collision");
        j["importedMask"]={{"path","recipe-assets/"+hash+".png"},{"sha256",hash}};
    }
    package::Validate(j,RecipeSchema(),"recipe",RecipeSchema());const auto text=j.dump(2)+"\n";
    Require(text.size()<=4*1024*1024,"recipe exceeds the 4 MiB JSON reader limit");
    AtomicWrite(path,std::span(reinterpret_cast<const uint8_t*>(text.data()),text.size()),overwrite);
}
RelightingRecipe LoadRecipe(const std::filesystem::path& input){
    const auto path=std::filesystem::absolute(input);RelightingRecipe r;r.document=package::ReadJson(path);const auto& j=r.document;
    package::Validate(j,RecipeSchema(),"recipe",RecipeSchema());
    // Package references may use ../ to a sibling package. Assets inside that package still use
    // the existing canonical path boundary. This is a read-only reference, never a write target.
    const auto root=std::filesystem::canonical(path.parent_path()/Relative(j.at("sourcePackage").get<std::string>()));
    r.package=std::make_shared<ScenePackage>(ScenePackageLoader{}.Load(root));Require(r.package->observation!=nullptr,"package lacks source observation");
    Require(Identity(*r.package->observation)==j.at("identity"),"source/anchor/analysis hash or backend revision mismatch");
    if(!j.at("importedMask").is_null()){const auto& m=j.at("importedMask");const auto mask=ConstrainAssetPath(path.parent_path()/Relative(m.at("path").get<std::string>()),path.parent_path());
        r.importedMask=LoadProtectionMask(mask);Require(r.importedMask->metadata.at("sha256")==m.at("sha256"),"imported mask hash mismatch");}
    r.session.Publish(r.package->observation);ApplyRecipeState(r.session,j.at("state"));return r;
}
}
