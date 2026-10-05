#include "ScenePackage/AppearanceAnchor.h"
#include "ScenePackage/AnchorImage.h"
#include "ScenePackage/PackageManifest.h"
#include "EmbeddedSchema.h"
#include <cmath>
namespace isr::package {
namespace {
void Require(bool value,const std::string& reason){if(!value)throw std::runtime_error("AppearanceAnchor: "+reason);}
void Close(const Json& actual,const Json& expected,const std::string& name){
    Require(actual.size()==expected.size(),name+" shape mismatch");
    for(size_t i=0;i<actual.size();++i){const double a=actual[i],b=expected[i];
        Require(std::abs(a-b)<=std::max(1e-7,1e-6*std::max(std::abs(a),std::abs(b))),name+" does not match the image coordinate contract");}
}
Json Orientation(int orientation,double w,double h){
    const double transforms[8][9]={{1,0,0,0,1,0,0,0,1},{-1,0,w,0,1,0,0,0,1},{-1,0,w,0,-1,h,0,0,1},{1,0,0,0,-1,h,0,0,1},
        {0,1,0,1,0,0,0,0,1},{0,-1,h,1,0,0,0,0,1},{0,-1,h,-1,0,w,0,0,1},{0,1,0,-1,0,w,0,0,1}};
    return transforms[orientation-1];
}
}
std::optional<AppearanceAnchor> ReadAppearanceAnchor(const std::filesystem::path& root){
    constexpr auto relative="relighting/relighting.json";const auto candidate=root/relative;
    if(!std::filesystem::exists(candidate)&&!std::filesystem::is_symlink(candidate))return {};
    try {
        static const auto schema=ParseSchema(AppearanceSchemaText);
        auto data=ReadJson(AssetPath(root,relative,"relighting",{".json"}));Validate(data,schema,"relighting",schema);
        AppearanceAnchor result;const auto& source=data["sourceImage"];const auto& analysis=data["analysisImage"];
        result.sourcePath=ValidateAnchorImage(root,source,false,&result.pixels);result.analysisPath=ValidateAnchorImage(root,analysis);
        result.sourceSize=source["size"].get<std::array<uint32_t,2>>();result.analysisSize=analysis["size"].get<std::array<uint32_t,2>>();
        const double sw=result.sourceSize[0],sh=result.sourceSize[1],aw=result.analysisSize[0],ah=result.analysisSize[1];
        Require(source["path"]!=analysis["path"]&&analysis["path"]=="textures/original_image.png","source anchor must be separate from processed original_image.png");
        Require(aw<=2048&&ah<=2048&&aw<=sw&&ah<=sh,"invalid analysis dimensions");
        Require(aw/ah>=.01&&aw/ah<=100,"analysis aspect outside v1 limits");
        const auto& mapping=data["analysisMapping"];Close(mapping["sourceToAnalysis"],Json{aw/sw,0,0,0,ah/sh,0,0,0,1},"sourceToAnalysis");
        const bool full=data["sourceKind"]=="canonical";const auto& norm=data["normalization"];std::string identity;
        if(full){
            Require(data.contains("originalFile")&&norm["pipeline"]=="pillow-exif-icc-white-rgb8-v1","canonical provenance is incomplete");
            ValidateAnchorImage(root,data["originalFile"],true);const auto& stored=data["originalFile"]["storedSize"];
            const int orientation=norm["exifOrientation"];const double w=stored[0],h=stored[1];
            Require(sw==(orientation>=5?h:w)&&sh==(orientation>=5?w:h),"canonical dimensions disagree with EXIF orientation");
            Close(norm["storedToCanonical"],Orientation(orientation,w,h),"storedToCanonical");
            Require(mapping["filter"]=="pillow-lanczos","canonical resize filter is missing");
            identity="original-sha256:"+data["originalFile"]["sha256"].get<std::string>();
        }else{
            Require(!data.contains("originalFile")&&norm["pipeline"]=="legacy-processed-unknown","legacy anchor cannot claim original provenance");
            Require(source["size"]==analysis["size"]&&mapping["filter"]=="legacy-unknown","legacy anchor must keep processed dimensions");
            Require(source["sha256"]==analysis["sha256"],"legacy anchor must preserve processed image bytes");
            identity="legacy-processed-sha256:"+source["sha256"].get<std::string>();
        }
        Require(data["sourceId"]==identity,"sourceId disagrees with source provenance");
        const auto& camera=data["sourceCamera"];const bool available=camera["status"]!="calibration-required";
        Require(full||!available,"legacy package needs source-camera calibration");
        if(available){
            const auto& k=camera["intrinsicsNormalized"];Require(k[0].get<double>()>0&&k[4].get<double>()>0,"focal lengths must be positive");
            Close(Json{k[1],k[3],k[6],k[7],k[8]},Json{0,0,0,0,1},"zero-skew intrinsics");
            Json ks=Json::array(),ka=Json::array();for(size_t i=0;i<9;++i){ks.push_back(k[i].get<double>()*(i<3?sw:i<6?sh:1));ka.push_back(k[i].get<double>()*(i<3?aw:i<6?ah:1));}
            Close(camera["sourceIntrinsicsPixels"],ks,"source intrinsics");Close(camera["analysisIntrinsicsPixels"],ka,"analysis intrinsics");
            Close(camera["cameraToWorld"],Json{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1},"reconstruction camera pose");
            Require((camera["status"]=="synthetic")== (camera["scaleType"]=="synthetic"),"camera scale/provenance disagree");
        }
        Require(data["capabilities"]==Json{{"fullResolutionAnchor",full},{"sourceCameraAvailable",available},
            {"requiresCalibration",!available||camera["status"]=="synthetic"}},"capabilities disagree with evidence");
        result.metadata=std::move(data);return result;
    }catch(const std::exception& error){throw std::runtime_error(std::string("Invalid optional ")+relative+": "+error.what());}
}
}
