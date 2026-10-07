#include "ScenePackage/SourceGeometry.h"
#include <cstring>
#include <iostream>
using namespace isr;
static void Require(bool b,const char* s){if(!b)throw std::runtime_error(s);}
int main(){try{
    SourceObservation source;Require(!BuildSourceGeometry(source).available,"Missing evidence must be unavailable");
    auto maps=std::make_shared<AnalysisMaps>();auto intrinsic=std::make_shared<IntrinsicData>();auto shadow=std::make_shared<ShadowData>();auto light=std::make_shared<LightingData>();
    light->metadata={{"fit",{{"confidence",1},{"backend","manual-test"}}}};
    auto image=[](NumericFormat format){NumericImage im{5,3,format,{}};im.bytes.resize(15*im.Channels()*4);return im;};
    auto f=[](NumericImage& im,size_t i,float v,unsigned c=0){std::memcpy(im.bytes.data()+(i*im.Channels()+c)*4,&v,4);};
    for(size_t i:{1,3,4,5,6,7,8,9,10})maps->maps[i].image=image(i==1||i==3||i==6?NumericFormat::Vector:i==4||i==5?NumericFormat::Label:NumericFormat::Float);
    for(size_t i=0;i<6;++i)intrinsic->maps[i]=image(i==5?NumericFormat::Label:i==4?NumericFormat::Float:NumericFormat::Vector);
    for(auto& im:shadow->maps)im=image(NumericFormat::Float);
    for(size_t i=0;i<15;++i){uint32_t label=1;
        for(size_t m:{4,5})std::memcpy(maps->maps[m].image->bytes.data()+i*4,&label,4);
        std::memcpy(intrinsic->maps[5]->bytes.data()+i*4,&label,4);
        f(*maps->maps[1].image,i,-1,2);f(*maps->maps[3].image,i,float(i%5)*.02f,0);f(*maps->maps[3].image,i,-float(i/5)*.02f,1);f(*maps->maps[3].image,i,3,2);
        f(*maps->maps[7].image,i,.6f);for(size_t m:{9,10})f(*maps->maps[m].image,i,1);f(shadow->maps[1],i,1);
        for(unsigned c=0;c<3;++c)f(*maps->maps[6].image,i,.4f,c);
    }
    source.analysisMaps=maps;source.intrinsic=intrinsic;source.shadow=shadow;source.lighting=light;
    auto full=BuildSourceGeometry(source);Require(full.available&&full.scene.meshes[0].indices.size()==48,"Odd grid must create 16 triangles");
    Require(full.report["receiverPixels"]==15,"Known receiver support");
    uint32_t zero=0;std::memcpy(maps->maps[4].image->bytes.data()+7*4,&zero,4);
    auto hole=BuildSourceGeometry(source);Require(hole.scene.meshes[0].indices.size()<48&&hole.evidence.FloatAt(7,3)==0,"Invalid receiver cannot bridge hole");
    f(*maps->maps[3].image,0,100,2);auto step=BuildSourceGeometry(source);Require(step.evidence.FloatAt(0,3)==0,"Large depth edge excluded");
    Require(full.scene.meshes[0].vertices[0].position.z==3,"Snapshot must own old vertices");
    f(*maps->maps[3].image,1,1e30f,0);Require(BuildSourceGeometry(source).evidence.FloatAt(1,3)==0,"Finite but extreme coordinates must not reach shadow matrices");
    shadow->excludedLabels.push_back(1);Require(!BuildSourceGeometry(source).available,"Excluded sky/emission regions cannot become occluders");
    std::cout<<"Source shell: missing/odd grid/holes/depth edge/immutable snapshot pass\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
