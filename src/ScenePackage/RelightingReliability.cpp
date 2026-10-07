#include "ScenePackage/RelightingReliability.h"
#include "ScenePackage/AnchorImage.h"
#include "Assets/AssetIO.h"
#include "Assets/ImageDecoder.h"
#include <cmath>
#include <cstring>
namespace isr {
std::shared_ptr<const ProtectionMask> LoadProtectionMask(const std::filesystem::path& path){
    if(path.empty())return {};
    if(std::filesystem::file_size(path)>16ull*1024*1024)throw std::runtime_error("Protection mask exceeds 16 MiB");
    const auto bytes=ReadAssetFile(path);constexpr uint8_t png[]{137,80,78,71,13,10,26,10};
    if(bytes.size()<8||!std::equal(std::begin(png),std::end(png),bytes.begin()))throw std::runtime_error("Protection mask must be a grayscale PNG");
    const auto decoded=DecodeImage(bytes,PathUtf8(path),2048ull*2048);
    if(decoded->width>2048||decoded->height>2048)throw std::runtime_error("Protection mask edge limit is 2048");
    auto mask=std::make_shared<ProtectionMask>();auto& im=mask->image;im.width=decoded->width;im.height=decoded->height;im.format=NumericFormat::Float;im.bytes.resize(size_t(im.width)*im.height*4);
    for(size_t p=0;p<size_t(im.width)*im.height;++p){const auto* rgb=decoded->rgba.data()+p*4;
        if(rgb[0]!=rgb[1]||rgb[1]!=rgb[2]||rgb[3]!=255)throw std::runtime_error("Protection mask requires opaque gray pixels (white=protect, black=allow)");
        const float value=float(rgb[0])/255;std::memcpy(im.bytes.data()+p*4,&value,4);}
    mask->metadata={{"path",PathUtf8(std::filesystem::absolute(path))},{"sha256",package::Sha256(bytes)},{"size",{im.width,im.height}},
        {"mapping","normalized-source-nearest"},{"meaning","white=protect; black=allow; data, not sRGB"}};return mask;
}
RelightingReliability BuildRelightingReliability(const AnalysisMaps* maps,const LightingData* lighting,uint32_t width,uint32_t height,const IntrinsicData* iid){
    RelightingReliability result;
    for(auto& im:result.weights){im.width=width;im.height=height;im.format=NumericFormat::Vector;im.bytes.resize(size_t(width)*height*16);}
    auto exists=[&](size_t i){return maps&&maps->maps[i].image.has_value();};
    auto scalar=[&](size_t i,size_t p,float fallback){return exists(i)?maps->maps[i].image->FloatAt(p):fallback;};
    auto label=[&](size_t i,size_t p){return exists(i)?maps->maps[i].image->UintAt(p):0u;};
    auto provenance=[&](size_t i){return maps&&maps->maps[i].metadata.is_object()?maps->maps[i].metadata.value("provenance",package::Json::object()):package::Json::object();};
    const auto geometry=provenance(9),material=provenance(10);
    const bool synthetic=geometry.value("kind","")=="synthetic";
    const bool binary=geometry.value("meaning","").find("validity")!=std::string::npos;
    const bool fallback=material.value("kind","")=="fallback";
    const bool manual=lighting&&lighting->metadata["fit"]["backend"]=="manual-test";
    const bool intrinsic=lighting&&lighting->metadata["fit"]["albedoSource"]=="intrinsic";
    auto put=[&](size_t map,size_t p,unsigned c,float value){std::memcpy(result.weights[map].bytes.data()+(p*4+c)*4,&value,4);};
    for(uint32_t y=0;y<height;++y)for(uint32_t x=0;x<width;++x){const size_t p=size_t(y)*width+x;
        const bool valid=exists(1)&&label(4,p)!=0;float geo=synthetic?1:binary?.8f:exists(9)?.25f+.75f*scalar(9,p,0):.5f;
        float boundary=exists(5)?(.5f+.5f*scalar(11,p,0)):.5f;
        for(const auto offset:{std::array<int,2>{-1,0},{1,0},{0,-1},{0,1}}){const int nx=int(x)+offset[0],ny=int(y)+offset[1];if(nx<0||ny<0||nx>=int(width)||ny>=int(height))continue;
            const size_t q=size_t(ny)*width+size_t(nx);float cosine=0;
            if(exists(1))for(unsigned c=0;c<3;++c)cosine+=maps->maps[1].image->FloatAt(p,c)*maps->maps[1].image->FloatAt(q,c);
            const float z=scalar(0,p,0),other=scalar(0,q,0);
            if(label(4,q)==0||label(5,p)!=label(5,q)||cosine<.85f||std::abs(z-other)>.1f*std::max(std::min(z,other),1e-6f))boundary=std::min(boundary,.35f);
        }
        float mat=fallback?.75f:exists(10)?.35f+.65f*scalar(10,p,0):.5f;
        // MR are behavior estimates, not truth. Strong reflection suspects are protected; missing MR stays conservative.
        if(exists(7)&&exists(8))mat*=std::clamp((scalar(7,p,.5f)-.08f)/.32f,0.f,1.f)*std::clamp((.85f-scalar(8,p,0))/.6f,0.f,1.f);
        else mat=std::min(mat,.5f);
        float fit=.5f,shadow=.8f;
        if(manual)fit=1;
        else if(lighting&&lighting->maps[3].UintAt(p))fit=std::exp(-lighting->maps[2].FloatAt(p)/.35f);
        if(lighting&&intrinsic&&lighting->maps[3].UintAt(p)){
            constexpr float luma[]{.2126f,.7152f,.0722f};float proxy=0,old=0;
            for(unsigned c=0;c<3;++c){proxy+=lighting->maps[0].FloatAt(p,c)*luma[c];old+=lighting->maps[1].FloatAt(p,c)*luma[c];}
            shadow=std::clamp((proxy/(old+.02f)-.2f)/.6f,.25f,1.f);
        }
        put(0,p,0,valid?geo:0);put(0,p,1,boundary);put(0,p,2,mat);put(0,p,3,fit);put(1,p,0,shadow);
        float support=1,fraction=0;
        if(iid){float rmax=0,predicted=0,minAlbedo=1;
            for(unsigned c=0;c<3;++c){const float a=iid->maps[0]->FloatAt(p,c),s=iid->maps[1]->FloatAt(p,c);
                const float r=iid->maps[2]?std::abs(iid->maps[2]->FloatAt(p,c)):0;
                rmax=std::max(rmax,r);predicted=std::max(predicted,a*s+r);minAlbedo=std::min(minAlbedo,a);}
            fraction=rmax/(predicted+.02f);
            support=std::exp(-iid->maps[4]->FloatAt(p)/.15f)*std::exp(-iid->maps[3]->FloatAt(p,1)/.15f)*std::clamp(1-fraction/.5f,0.f,1.f);
            if(!iid->maps[5]->UintAt(p)||minAlbedo<.03f)support=0;
        }
        put(1,p,1,support);put(1,p,2,std::min(fraction,1.f));put(1,p,3,iid?1.f:0.f);
    }
    result.provenance={{"geometry",geometry},{"material",material},{"region",provenance(11)},
        {"normal","geometry normal only; tangent fallback confidence never gates the surface"},
        {"combination","hard validity * (1-protection) * min(geometry,boundary,material,fit,signal,shadow-risk)"},
        {"defaults",{{"missingGeometryQuality",.5},{"binaryGeometryValidity",.8},{"neutralMaterial",.75},{"missingMaterial",.5},{"missingFit",.5},{"unknownShadowRisk",.8}}},
        {"meaning","independent heuristic weights, not calibrated joint probabilities; shadow risk is not cast-shadow detection"}};
    return result;
}
}
