#include "ScenePackage/SpecularHandling.h"
#include <algorithm>
#include <cmath>
#include <cstring>
namespace isr {
namespace {
using V=std::array<float,3>;
float Dot(V a,V b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
V Unit(V v){const float length=std::sqrt(std::max(Dot(v,v),1e-12f));for(float& x:v)x/=length;return v;}
V Read(const NumericImage& im,size_t p){return {im.FloatAt(p,0),im.FloatAt(p,1),im.FloatAt(p,2)};}
float Y(V v){return .2126f*v[0]+.7152f*v[1]+.0722f*v[2];}
void Put(NumericImage& im,size_t p,unsigned c,float v){std::memcpy(im.bytes.data()+(p*4+c)*4,&v,4);}
float Decode(uint8_t v){float x=float(v)/255;return x<=.04045f?x/12.92f:std::pow((x+.055f)/1.055f,2.4f);}
}
V EvaluateImageGgx(const V& normal,const V& position,const V& albedo,float roughness,float metallic,const LightingParameters& light){
    const V n=Unit(normal),v=Unit({-position[0],-position[1],-position[2]}),l=Unit({-light.direction[0],-light.direction[1],-light.direction[2]});
    const float nl=std::clamp(Dot(n,l),0.f,1.f),nv=std::clamp(Dot(n,v),0.f,1.f);if(nl<=0||nv<=0)return {};
    const V h=Unit({v[0]+l[0],v[1]+l[1],v[2]+l[2]});const float nh=std::clamp(Dot(n,h),0.f,1.f),vh=std::clamp(Dot(v,h),0.f,1.f);
    const float a=std::pow(std::max(roughness,.045f),2.f),a2=a*a,den=nh*nh*(a2-1)+1;
    const float distribution=a2/(3.14159265359f*den*den);
    auto g=[&](float x){return 2*x/std::max(x+std::sqrt(a2+(1-a2)*x*x),1e-7f);};
    const float common=distribution*g(nl)*g(nv)/std::max(4*nl*nv,1e-7f)*nl*3.14159265359f;
    V result{};for(unsigned c=0;c<3;++c){const float f0=.04f*(1-metallic)+albedo[c]*metallic;
        result[c]=(f0+(1-f0)*std::pow(1-vh,5.f))*common*light.directColor[c]*light.directIntensity;}return result;
}
SpecularEvidence BuildSpecularEvidence(const SourceObservation& source){
    SpecularEvidence result;const auto size=source.anchor?source.anchor->analysisSize:std::array<uint32_t,2>{1,1};
    for(auto* im:{&result.candidate,&result.protectedResidual}){im->width=size[0];im->height=size[1];im->format=NumericFormat::Vector;im->bytes.resize(size_t(size[0])*size[1]*16);}
    result.metadata={{"available",false},{"reason","Requires fixed geometry, lighting, intrinsic residual and material evidence"}};
    if(!source.anchor||!source.analysisMaps||!source.lighting||!source.intrinsic||!source.intrinsic->maps[2])return result;
    const auto& maps=source.analysisMaps->maps;for(size_t i:{1,3,4,6,7,8,10})if(!maps[i].image)return result;
    const auto& iid=*source.intrinsic;const auto count=size_t(size[0])*size[1];
    std::vector<V> calculated(count);std::vector<float> ratios;
    auto eligible=[&](size_t p){return maps[4].image->UintAt(p)&&iid.maps[5]->UintAt(p)&&source.lighting->maps[3].UintAt(p)
        &&maps[7].image->FloatAt(p)>=.2f&&maps[7].image->FloatAt(p)<=.8f&&maps[8].image->FloatAt(p)<=.3f&&maps[10].image->FloatAt(p)>.2f;};
    for(size_t p=0;p<count;++p){calculated[p]=EvaluateImageGgx(Read(*maps[1].image,p),Read(*maps[3].image,p),Read(*maps[6].image,p),maps[7].image->FloatAt(p),maps[8].image->FloatAt(p),source.lighting->source);
        const V residual=Read(*iid.maps[2],p);const float old=Y(calculated[p]),r=Y(residual);
        if(eligible(p)&&old>.005f&&r>.002f&&r<.5f)ratios.push_back(r/old);}
    // Only one bounded global scale is fitted. Local agreement rejects unexplained residuals.
    if(ratios.size()>=16){auto middle=ratios.begin()+ratios.size()/2;std::nth_element(ratios.begin(),middle,ratios.end());result.scale=std::clamp(*middle,.05f,4.f);result.available=true;}
    const auto& pixels=*source.anchor->pixels;uint64_t supported=0;double maxClip=0;
    for(size_t p=0;p<count;++p){const V residual=Read(*iid.maps[2],p);V old=calculated[p];for(float& c:old)c*=result.scale;
        const float ry=Y(residual),sy=Y(old);float confidence=0;
        if(result.available&&eligible(p)&&*std::min_element(residual.begin(),residual.end())>=0&&ry<=3*sy+.02f){
            confidence=std::clamp(maps[10].image->FloatAt(p),0.f,1.f)*std::exp(-iid.maps[4]->FloatAt(p)/.1f)
                *std::exp(-iid.maps[3]->FloatAt(p,2)/.1f)*std::exp(-3*std::abs(ry-sy)/(.02f+ry+sy));}
        const uint32_t x=uint32_t(p%size[0]),y=uint32_t(p/size[0]);
        const auto sx=std::min(uint32_t((double(x)+.5)*pixels.width/size[0]),pixels.width-1),syPixel=std::min(uint32_t((double(y)+.5)*pixels.height/size[1]),pixels.height-1);
        float clip=0;
        for(unsigned c=0;c<3;++c){const float original=Decode(pixels.rgba[(size_t(syPixel)*pixels.width+sx)*4+c]);
            const float proposed=std::max(0.f,std::min(residual[c],old[c]))*confidence,component=std::min(proposed,.6f*original);
            clip=std::max(clip,proposed-component);Put(result.candidate,p,c,component);Put(result.protectedResidual,p,c,residual[c]-component);}
        Put(result.candidate,p,3,confidence);Put(result.protectedResidual,p,3,clip);maxClip=std::max(maxClip,double(clip));if(confidence>.1f)++supported;
    }
    result.metadata={{"available",result.available},{"reason",result.available?"Heuristic dielectric directional support; not confirmed specular":"Fewer than 16 scale-supported samples; previous diffuse path retained"},
        {"scale",result.scale},{"scaleSamples",ratios.size()},{"supportedPixels",supported},{"maxClip",maxClip},
        {"coordinates","fixed LH camera; view=normalize(-pointMap)"},{"units","GGX incident coefficient = PI * relative Lambert direct; bounded global scale"},
        {"limits","roughness [.2,.8], metallic<=.3; unknown/reflection/emission protected by fit mask and residual disagreement; no glass or environment reconstruction"}};
    return result;
}
}
