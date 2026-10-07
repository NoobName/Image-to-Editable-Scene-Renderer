#include "ScenePackage/IntrinsicData.h"
#include "ScenePackage/PackageManifest.h"
#include "ScenePackage/AnchorImage.h"
#include "Assets/AssetIO.h"
#include "Assets/ImageDecoder.h"
#include "EmbeddedSchema.h"
#include <cmath>
namespace isr {
namespace {
void Require(bool b,const std::string& reason){if(!b)throw std::runtime_error(reason);}
std::vector<uint8_t> Fingerprint(const std::filesystem::path& root,const package::Json& r,bool input){
    const auto path=package::AssetPath(root,r["path"],input?"textures":"intrinsic",input?std::initializer_list<std::string>{".png"}:std::initializer_list<std::string>{".dds"});
    Require(std::filesystem::file_size(path)<=128ull*1024*1024,"intrinsic asset exceeds byte budget");
    auto bytes=ReadAssetFile(path);Require(bytes.size()<=128ull*1024*1024&&package::Sha256(bytes)==r["sha256"].get<std::string>(),"intrinsic hash mismatch");return bytes;
}
}
std::shared_ptr<const IntrinsicData> LoadIntrinsicData(const std::filesystem::path& root,const std::optional<package::AppearanceAnchor>& anchor){
    if(!std::filesystem::exists(root/"intrinsic/intrinsic.json"))return {};
    try{
        static const auto schema=package::ParseSchema(package::IntrinsicSchemaText);auto result=std::make_shared<IntrinsicData>();auto& d=result->metadata;
        d=package::ReadJson(package::AssetPath(root,"intrinsic/intrinsic.json","intrinsic",{".json"}));package::Validate(d,schema,"$intrinsic",schema);
        Require(anchor.has_value(),"intrinsic requires source anchor");const auto& a=anchor->metadata;
        Require(d["sourceId"]==a["sourceId"]&&d["sourceSha256"]==a["sourceImage"]["sha256"]&&d["analysisSize"]==a["analysisImage"]["size"],"intrinsic source identity/size mismatch");
        Require(d["input"]["path"]=="textures/original_image.png","intrinsic observation path mismatch");
        const auto input=Fingerprint(root,d["input"],true);const auto original=DecodeImage(input,"intrinsic observation",2048ull*2048);
        const uint32_t w=d["analysisSize"][0],h=d["analysisSize"][1];Require(original->width==w&&original->height==h,"intrinsic observation size mismatch");
        for(size_t i=0;i<IntrinsicKeys.size();++i){const auto key=IntrinsicKeys[i];if(!d["maps"].contains(key))continue;
            const auto& r=d["maps"][key];const auto bytes=Fingerprint(root,r,false);
            auto image=LoadNumericDds(package::AssetPath(root,r["path"],"intrinsic",{".dds"}));
            const auto format=i==5?NumericFormat::Label:i==4?NumericFormat::Float:NumericFormat::Vector;
            Require(image.width==w&&image.height==h&&image.format==format&&r["format"]==(i==5?"R32_UINT":i==4?"R32_FLOAT":"RGBA32_FLOAT"),"intrinsic format/size mismatch");
            Require(bytes.size()==148+image.bytes.size()&&std::equal(image.bytes.begin(),image.bytes.end(),bytes.begin()+148),"intrinsic DDS changed during load");
            const double lo=i==2&&d["residualSemantics"]=="signed-non-diffuse"?-64:0,hi=(i==0||i==5)?1:i==4?128:64;
            for(size_t p=0;p<size_t(w)*h;++p)for(unsigned c=0;c<image.Channels();++c){const double v=i==5?double(image.UintAt(p)):image.FloatAt(p,c);
                Require(v>=lo&&v<=hi&&(c!=3||v==0),"intrinsic value/range mismatch");}
            result->maps[i]=std::move(image);
        }
        Require(bool(result->maps[2])==(d["residualSemantics"]!="unavailable")&&d["residualSemantics"]==d["provenance"]["residual_semantics"]&&d["gauge"]["name"]==d["provenance"]["gauge"]&&d["gauge"]["scaleAmbiguous"]==d["provenance"]["scale_ambiguous"],"intrinsic semantics/gauge mismatch");
        // Independent recomposition check uses the fixed processed RGB bytes, never Material texture previews.
        uint64_t count=0;double sum=0,maximum=0;
        for(size_t p=0;p<size_t(w)*h;++p){double square=0;
            for(unsigned c=0;c<3;++c){double v=double(original->rgba[p*4+c])/255;v=v<=.04045?v/12.92:std::pow((v+.055)/1.055,2.4);
                const double reconstructed=double(result->maps[0]->FloatAt(p,c))*result->maps[1]->FloatAt(p,c)+(result->maps[2]?result->maps[2]->FloatAt(p,c):0);
                square+=(v-reconstructed)*(v-reconstructed)/3;}
            const double error=std::sqrt(square);Require(std::abs(error-result->maps[4]->FloatAt(p))<=1e-5+1e-5*error,"intrinsic recomposition error mismatch");
            if(result->maps[5]->UintAt(p)){++count;sum+=square;maximum=std::max(maximum,error);}}
        const double rmse=count?std::sqrt(sum/double(count)):0;
        Require(d["metrics"]["validPixels"]==count&&std::abs(d["metrics"]["rmse"].get<double>()-rmse)<1e-5&&std::abs(d["metrics"]["maxError"].get<double>()-maximum)<1e-5,"intrinsic statistics mismatch");
        return result;
    }catch(const std::exception& e){throw std::runtime_error(std::string("Invalid optional intrinsic/intrinsic.json: ")+e.what());}
}
}
