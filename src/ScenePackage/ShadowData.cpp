#include "ScenePackage/ShadowData.h"
#include "ScenePackage/RelightingReliability.h"
#include "ScenePackage/PackageManifest.h"
#include "ScenePackage/AnchorImage.h"
#include "Assets/AssetIO.h"
#include "EmbeddedSchema.h"
#include <cmath>
namespace isr {
namespace {void Require(bool b,const std::string& s){if(!b)throw std::runtime_error("Shadow: "+s);}}
std::shared_ptr<const ShadowData> LoadShadowData(const std::filesystem::path& root,const std::optional<package::AppearanceAnchor>& appearance){
    if(!std::filesystem::exists(root/"shadow/shadow.json"))return {};
    try{
        static const auto schema=package::ParseSchema(package::ShadowSchemaText);auto result=std::make_shared<ShadowData>();auto& j=result->metadata;
        j=package::ReadJson(package::AssetPath(root,"shadow/shadow.json","shadow",{".json"}));package::Validate(j,schema,"$shadow",schema);
        Require(appearance.has_value(),"source anchor required");const auto& a=appearance->metadata;
        Require(j["sourceId"]==a["sourceId"]&&j["sourceSha256"]==a["sourceImage"]["sha256"]&&j["analysisSize"]==a["analysisImage"]["size"],"source identity/dimensions mismatch");
        auto fingerprint=[&](const package::Json& r){const auto relative=r["path"].get<std::string>();const auto folder=relative.substr(0,relative.find('/'));
            auto path=package::AssetPath(root,relative,folder,{".json",".dds",".png"});Require(std::filesystem::file_size(path)<=128ull*1024*1024,"asset budget");
            const auto bytes=ReadAssetFile(path);Require(r["sha256"].get<std::string>().size()==64&&package::Sha256(bytes)==r["sha256"].get<std::string>(),"stale dependency/hash: "+relative);return path;};
        constexpr const char* inputs[]{"relighting/relighting.json","analysis/analysis.json","intrinsic/intrinsic.json","lighting/lighting.json",
            "analysis/depth.dds","analysis/normal.dds","analysis/position.dds","analysis/validity.dds","analysis/region.dds","analysis/roughness.dds","analysis/metallic.dds"};
        for(size_t i=0;i<std::size(inputs);++i){Require(j["inputs"][i]["path"]==inputs[i],"input list mismatch");fingerprint(j["inputs"][i]);}
        const auto scene=package::ReadJson(root/"scene.json");std::vector<std::string> regions;
        for(const auto& object:scene["objects"])if(object.contains("region"))regions.push_back(object["region"].get<std::string>());
        std::sort(regions.begin(),regions.end());regions.erase(std::unique(regions.begin(),regions.end()),regions.end());
        Require(j["regionInputs"].size()==regions.size(),"region source count mismatch");
        for(size_t i=0;i<regions.size();++i){Require(j["regionInputs"][i]["path"]==regions[i],"region source list mismatch");package::AssetPath(root,regions[i],"objects",{".json"});
            const auto path=fingerprint(j["regionInputs"][i]);const auto bytes=ReadAssetFile(path);
            Require(package::Sha256(bytes)==j["regionInputs"][i]["sha256"].get<std::string>(),"region changed during validation");
            const auto region=package::Json::parse(bytes);
            for(const auto& excluded:j["diagnostics"]["excludedNames"])if(region["name"]==excluded)result->excludedLabels.push_back(region["labelId"].get<uint32_t>());
        }
        const uint32_t w=j["analysisSize"][0],h=j["analysisSize"][1];
        for(size_t i=0;i<8;++i){const auto& record=j["maps"][ShadowKeys[i]];fingerprint(record);
            auto& image=result->maps[i];image=LoadNumericDds(package::AssetPath(root,record["path"],"shadow",{".dds"}));
            Require(image.width==w&&image.height==h&&image.format==(i==4?NumericFormat::Label:NumericFormat::Float)&&record["format"]==(i==4?"R32_UINT":"R32_FLOAT"),"map format/dimensions mismatch");
            const auto snapshot=ReadAssetFile(package::AssetPath(root,record["path"],"shadow",{".dds"}));
            Require(snapshot.size()==148+image.bytes.size()&&package::Sha256(snapshot)==record["sha256"].get<std::string>()&&std::equal(image.bytes.begin(),image.bytes.end(),snapshot.begin()+148),"numeric snapshot changed during validation");
            for(size_t p=0;p<size_t(w)*h;++p){const float v=i==4?float(image.UintAt(p)):image.FloatAt(p);Require(v>=0&&v<=1,"map range");}
        }
        for(size_t i=0;i<2;++i){const char* key=i?"protect":"confirm";const auto& image=result->maps[i+5];
            if(!j["manual"].contains(key)){for(size_t p=0;p<size_t(w)*h;++p)Require(image.FloatAt(p)==0,"manual data without provenance");continue;}
            const auto& r=j["manual"][key];auto path=fingerprint(r);path=package::AssetPath(root,r["path"],"shadow",{".png"});const auto mask=LoadProtectionMask(path);
            const auto& m=mask->image;Require(r["size"]==package::Json::array({m.width,m.height}),"manual mask size mismatch");
            for(uint32_t y=0;y<h;++y)for(uint32_t x=0;x<w;++x){const auto sx=uint32_t((double(x)+.5)*m.width/w),sy=uint32_t((double(y)+.5)*m.height/h);
                Require(std::abs(image.FloatAt(size_t(y)*w+x)-m.FloatAt(size_t(sy)*m.width+sx))<1e-7,"manual layer mismatch");}
        }
        uint64_t candidates=0,unknowns=0;
        for(size_t p=0;p<size_t(w)*h;++p){const float candidate=result->maps[0].FloatAt(p);const auto unknown=result->maps[4].UintAt(p);candidates+=candidate>0;unknowns+=unknown;
            Require(!unknown||candidate==0,"candidate in unknown region");Require(candidate==0||result->maps[2].FloatAt(p)>0,"candidate without geometry support");
            const float expected=std::max(candidate,result->maps[5].FloatAt(p))*(1-result->maps[6].FloatAt(p));Require(std::abs(expected-result->maps[7].FloatAt(p))<1e-6,"effective layer mismatch");}
        Require(j["diagnostics"]["candidatePixels"]==candidates&&j["diagnostics"]["unknownPixels"]==unknowns,"statistics mismatch");return result;
    }catch(const std::exception& e){throw std::runtime_error(std::string("Invalid optional shadow/shadow.json: ")+e.what());}
}
}
