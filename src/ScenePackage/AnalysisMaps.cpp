#include "ScenePackage/AnalysisMaps.h"
#include "ScenePackage/PackageManifest.h"
#include "EmbeddedSchema.h"
#include <cmath>
#include <algorithm>
namespace isr {
namespace {
void Require(bool ok,const std::string& message){if(!ok)throw std::runtime_error("AnalysisMaps: "+message);}
bool Close(double a,double b){return std::abs(a-b)<=1e-6+1e-5*std::abs(b);}
}
std::shared_ptr<const AnalysisMaps> LoadAnalysisMaps(const std::filesystem::path& root,const std::optional<package::AppearanceAnchor>& anchor){
    if(!std::filesystem::exists(root/"analysis/analysis.json"))return {};
    try{
        static const auto schema=package::ParseSchema(package::AnalysisSchemaText);
        auto result=std::make_shared<AnalysisMaps>();auto& data=result->metadata;
        data=package::ReadJson(package::AssetPath(root,"analysis/analysis.json","analysis",{".json"}));
        package::Validate(data,schema,"$analysis",schema);Require(anchor.has_value(),"validated source anchor required");
        const auto& a=anchor->metadata;
        Require(data["sourceId"]==a["sourceId"]&&data["sourceSize"]==a["sourceImage"]["size"]&&data["analysisSize"]==a["analysisImage"]["size"],"source identity/dimensions mismatch");
        for(size_t i=0;i<9;++i)Require(Close(data["sourceToAnalysis"][i],a["analysisMapping"]["sourceToAnalysis"][i]),"source mapping mismatch");
        const uint32_t w=data["analysisSize"][0],h=data["analysisSize"][1];const auto& k=data["camera"]["intrinsicsNormalized"];
        Require(w<=2048&&h<=2048&&uint64_t(w)*h<=4ull*1024*1024,"analysis dimensions exceed runtime budget");
        Require(k[0].get<double>()>0&&k[4].get<double>()>0,"positive focal lengths required");
        for(const auto i:{1,3,6,7,8})Require(Close(k[i],i==8?1:0),"zero-skew pinhole intrinsics required");
        for(size_t i=0;i<9;++i)Require(Close(data["camera"]["intrinsicsPixels"][i],k[i].get<double>()*(i<3?w:i<6?h:1)),"normalized/pixel intrinsics mismatch");
        for(size_t i=0;i<16;++i)Require(data["camera"]["cameraToWorld"][i]==(i%5==0?1:0),"only identity reconstruction world frame is supported");
        const std::string scale=data["camera"]["scaleType"],distanceUnit=scale=="metric"?"meters":scale+"-units";
        uint64_t total=0;
        for(size_t i=0;i<AnalysisKeys.size();++i){auto& map=result->maps[i];map.metadata=data["maps"][AnalysisKeys[i]];
            const auto& m=map.metadata;if(m["status"]=="unavailable")continue;
            const bool vector=i==1||i==2||i==3||i==6||i==12,integer=i==4||i==5,geometry=i<=3||i==9;
            const auto expected=integer?NumericFormat::Label:vector?NumericFormat::Vector:NumericFormat::Float;
            const char* format=integer?"R32_UINT":vector?"RGBA32_FLOAT":"R32_FLOAT";
            const char* space=i==0?"camera-z":i==1||i==3?"lh-camera":i==2?"lh-world":i==6?"linear-rgb":i==12?"tangent":"image";
            const std::string units=i==0||i==3?distanceUnit:i==1||i==2||i==12?"unit-vector":i==4?"binary":i==5?"label-id":i==6?"reflectance":i>=9&&i<=11?"score":"unitless";
            Require(m["format"]==format&&m["space"]==space&&m["units"]==units&&m["validity"]==(geometry?"geometry":i==11?"region-nonzero":"all-pixels")&&m["sampling"]==(integer?"nearest":"guarded-bilinear"),"semantic mismatch for "+std::string(AnalysisKeys[i]));
            const auto path=package::AssetPath(root,m["path"],"analysis",{".dds"});total+=std::filesystem::file_size(path);
            Require(total<=256ull*1024*1024,"256 MiB total map budget exceeded");map.image=LoadNumericDds(path);const auto& image=*map.image;
            Require(image.width==w&&image.height==h&&image.format==expected&&m["size"]==data["analysisSize"],"DDS dimensions/format disagree with metadata");
            double low=INFINITY,high=-INFINITY;
            for(size_t p=0;p<size_t(w)*h;++p)for(unsigned c=0;c<image.Channels();++c){
                const double v=integer?double(image.UintAt(p)):double(image.FloatAt(p,c));low=std::min(low,v);high=std::max(high,v);
                if(vector&&c==3)Require(v==0,"RGBA padding must be zero");
                if(i==4)Require(v==0||v==1,"validity must be binary");
                if(i>=6&&i<=11)Require(v>=0&&v<=1,"material/quality must lie in [0,1]");
            }
            Require(Close(m["range"][0],low)&&Close(m["range"][1],high),"map range mismatch");
        }
        const auto get=[&](size_t i){return result->maps[i].image?&*result->maps[i].image:nullptr;};
        for(const auto i:{0,1,2,3,9})if(get(i))Require(get(4)!=nullptr,"geometry maps require validity");
        if(get(3))Require(get(0)!=nullptr,"position requires depth");
        if(get(11))Require(get(5)!=nullptr,"region confidence requires labels");
        for(size_t p=0;p<size_t(w)*h;++p){const bool valid=get(4)&&get(4)->UintAt(p)!=0;
            for(const auto i:{0,1,2,3,9})if(get(i)&&!valid)for(unsigned c=0;c<get(i)->Channels();++c)Require(get(i)->FloatAt(p,c)==0,"invalid geometry must be zero");
            for(const auto i:{1,2,12})if(get(i)&&(valid||i==12)){double length=0;for(unsigned c=0;c<3;++c){const double v=get(i)->FloatAt(p,c);length+=v*v;}
                Require(std::abs(std::sqrt(length)-1)<=1e-4,"normal must have unit length");}
            if(get(1)&&get(2))for(unsigned c=0;c<3;++c)Require(Close(get(1)->FloatAt(p,c),get(2)->FloatAt(p,c)),"identity camera/world normals disagree");
            if(get(0)&&valid)Require(get(0)->FloatAt(p)>0&&get(0)->FloatAt(p)<=10000,"depth must be positive camera Z <=10000");
            if(get(3)){const double z=get(0)->FloatAt(p),u=(double(p%w)+.5)/w,v=(double(p/w)+.5)/h;
                const double expected[]{(u-k[2].get<double>())*z/k[0].get<double>(),-(v-k[5].get<double>())*z/k[4].get<double>(),z};
                for(unsigned c=0;c<3;++c)Require(std::abs(get(3)->FloatAt(p,c)-expected[c])<=1e-4+1e-4*std::abs(expected[c]),"position disagrees with LH half-center camera-Z projection");}
        }
        return result;
    }catch(const std::exception& e){throw std::runtime_error(std::string("Invalid optional analysis/analysis.json: ")+e.what());}
}
}
