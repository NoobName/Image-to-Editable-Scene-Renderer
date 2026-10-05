#include "ScenePackage/LightingData.h"
#include "ScenePackage/PackageManifest.h"
#include "ScenePackage/AnchorImage.h"
#include "Assets/AssetIO.h"
#include "EmbeddedSchema.h"
#include <algorithm>
#include <cmath>
namespace isr {
namespace {
void Require(bool ok,const std::string& reason){if(!ok)throw std::runtime_error("Lighting: "+reason);}
LightingParameters Parameters(const package::Json& j){
    LightingParameters p;p.direction=j["direction"].get<std::array<float,3>>();
    p.directColor=j["direct"]["color"].get<std::array<float,3>>();p.ambientColor=j["ambient"]["color"].get<std::array<float,3>>();
    p.directIntensity=j["direct"]["intensity"];p.ambientIntensity=j["ambient"]["intensity"];
    double length=0;for(float v:p.direction)length+=v*v;Require(std::abs(std::sqrt(length)-1)<1e-5,"direction must be unit length");return p;
}
std::vector<uint8_t> Fingerprint(const std::filesystem::path& root,const package::Json& record,const std::string& folder){
    const auto path=package::AssetPath(root,record["path"],folder,{".dds",".png"});
    Require(std::filesystem::file_size(path)<=128ull*1024*1024,"asset byte budget exceeded");
    const auto bytes=ReadAssetFile(path);Require(bytes.size()<=128ull*1024*1024,"asset byte budget exceeded");
    Require(package::Sha256(bytes)==record["sha256"].get<std::string>(),"stale input/cache hash: "+record["path"].get<std::string>());return bytes;
}
}
std::shared_ptr<const LightingData> LoadLightingData(const std::filesystem::path& root,const std::optional<package::AppearanceAnchor>& appearance){
    if(!std::filesystem::exists(root/"lighting/lighting.json"))return {};
    try{
        static const auto schema=package::ParseSchema(package::LightingSchemaText);
        auto result=std::make_shared<LightingData>();auto& data=result->metadata;
        data=package::ReadJson(package::AssetPath(root,"lighting/lighting.json","lighting",{".json"}));
        package::Validate(data,schema,"$lighting",schema);Require(appearance.has_value(),"validated source anchor required");
        const auto& a=appearance->metadata;
        Require(data["sourceId"]==a["sourceId"]&&data["sourceSha256"]==a["sourceImage"]["sha256"]&&data["analysisSize"]==a["analysisImage"]["size"],"source identity/dimensions mismatch");
        const uint32_t w=data["analysisSize"][0],h=data["analysisSize"][1];const auto& fit=data["fit"];
        Require(fit["totalPixels"]==w*h&&fit["validPixels"].get<uint64_t>()<=uint64_t(w)*h&&std::abs(fit["validFraction"].get<double>()-fit["validPixels"].get<double>()/(double(w)*h))<=1e-8,"fit support statistics mismatch");
        result->source=Parameters(data["sourceLighting"]);result->target=Parameters(data["targetLighting"]);
        constexpr const char* inputs[]={"textures/original_image.png","analysis/normal.dds","analysis/albedo.dds","analysis/validity.dds","analysis/roughness.dds","analysis/metallic.dds","analysis/region.dds","analysis/materialConfidence.dds"};
        std::vector<uint8_t> normalBytes;
        for(size_t i=0;i<std::size(inputs);++i){const auto& r=data["inputs"][i];Require(r["path"]==inputs[i],"input fingerprint list mismatch");
            auto bytes=Fingerprint(root,r,i==0?"textures":"analysis");if(i==1)normalBytes=std::move(bytes);}
        for(size_t i=0;i<LightingKeys.size();++i){const auto& r=data["maps"][LightingKeys[i]];const auto bytes=Fingerprint(root,r,"lighting");
            auto& image=result->maps[i];image=LoadNumericDds(package::AssetPath(root,r["path"],"lighting",{".dds"}));
            const auto format=i<2?NumericFormat::Vector:i==2?NumericFormat::Float:NumericFormat::Label;
            Require(image.width==w&&image.height==h&&image.format==format&&r["format"]==(i<2?"RGBA32_FLOAT":i==2?"R32_FLOAT":"R32_UINT"),"DDS format/dimensions mismatch");
            Require(bytes.size()==148+image.bytes.size()&&std::equal(image.bytes.begin(),image.bytes.end(),bytes.begin()+148),"DDS changed during load");
            for(size_t p=0;p<size_t(w)*h;++p)for(unsigned c=0;c<image.Channels();++c){
                const double v=i==3?double(image.UintAt(p)):double(image.FloatAt(p,c));
                Require(v>=0&&(i!=3||v<=1)&&(c!=3||v==0),"invalid lighting map values");}
        }
        const auto normal=LoadNumericDds(package::AssetPath(root,inputs[1],"analysis",{".dds"}));
        Require(normal.width==w&&normal.height==h&&normal.format==NumericFormat::Vector,"normal dimensions/format mismatch");
        Require(normalBytes.size()==148+normal.bytes.size()&&std::equal(normal.bytes.begin(),normal.bytes.end(),normalBytes.begin()+148),"normal changed during load");
        uint64_t supported=0;
        for(size_t p=0;p<size_t(w)*h;++p){if(result->maps[3].UintAt(p))++supported;
            else{Require(result->maps[2].FloatAt(p)==0,"residual outside fit mask must be zero");
                for(unsigned c=0;c<3;++c)Require(result->maps[0].FloatAt(p,c)==0,"proxy outside fit mask must be zero");}
            double length=0,cosine=0,residual=0;constexpr double luminance[]{.2126,.7152,.0722};const auto& light=result->source;
            for(unsigned c=0;c<3;++c){const double n=normal.FloatAt(p,c);length+=n*n;cosine-=n*light.direction[c];}
            for(unsigned c=0;c<3;++c){const double expected=length<.25?0:std::max(cosine,0.)*light.directIntensity*light.directColor[c]+light.ambientIntensity*light.ambientColor[c];
                Require(std::abs(result->maps[1].FloatAt(p,c)-expected)<=1e-5+1e-5*std::abs(expected),"old shading/source calibration mismatch");
                residual+=(double(result->maps[0].FloatAt(p,c))-result->maps[1].FloatAt(p,c))*luminance[c];}
            residual=result->maps[3].UintAt(p)?std::abs(residual):0;
            Require(std::abs(result->maps[2].FloatAt(p)-residual)<=1e-5+1e-5*residual,"residual/cache mismatch");}
        Require(supported==fit["validPixels"].get<uint64_t>(),"fit mask/support mismatch");
        Require(!fit["identifiable"].get<bool>()||(fit["albedoSource"]=="intrinsic"&&(fit["status"]=="fitted"||fit["status"]=="low-confidence")),"identifiability/provenance mismatch");
        const char* provenance=fit["backend"]=="manual-test"?"manual-test":fit["identifiable"].get<bool>()?"estimated-baseline":"unreliable-baseline";
        Require(data["sourceLighting"]["provenance"]==provenance&&(fit["identifiable"].get<bool>()||fit["confidence"].get<double>()<=.1),"source provenance/confidence mismatch");
        return result;
    }catch(const std::exception& e){throw std::runtime_error(std::string("Invalid optional lighting/lighting.json: ")+e.what());}
}
void LightingSession::Publish(const LightingData* data)noexcept{
    available=data!=nullptr;cacheValid=available;manualSource=false;sourceRevision=0;
    source=data?data->source:LightingParameters{};target=data?data->target:LightingParameters{};draft=source;
}
bool LightingSession::ApplySource()noexcept{
    if(!available)return false;
    auto valid=[](float x,float high){return std::isfinite(x)&&x>=0&&x<=high;};
    double length=0;for(float v:draft.direction){if(!std::isfinite(v))return false;length+=double(v)*v;}
    if(length<1e-12||!valid(draft.directIntensity,64)||!valid(draft.ambientIntensity,64))return false;
    for(auto& color:{draft.directColor,draft.ambientColor})for(float c:color)if(!valid(c,1))return false;
    source=draft;for(float& v:source.direction)v=float(v/std::sqrt(length));draft=source;
    // The old fit is tied to the previous source light. Descriptors stay alive; rendering marks it unavailable.
    cacheValid=false;manualSource=true;++sourceRevision;return true;
}
}
