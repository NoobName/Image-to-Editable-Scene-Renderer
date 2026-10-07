#include "ScenePackage/SourceGeometry.h"
#include <algorithm>
#include <cmath>
#include <cstring>
namespace isr {
SourceGeometry BuildSourceGeometry(const SourceObservation& source){
    SourceGeometry out;out.evidence={1,1,NumericFormat::Vector,std::vector<uint8_t>(16)};
    out.report={{"available",false},{"reason","Requires fixed position/normal/validity/region, intrinsic and old-shadow evidence"},
        {"space","LH source camera; identity world"},{"policy","Observed front surfaces only; no hidden, back-face or offscreen completion"}};
    if(!source.analysisMaps||!source.intrinsic||!source.shadow||!source.lighting)return out;
    const auto& maps=source.analysisMaps->maps;
    for(size_t i:{1,3,4,5,6,7,8,9,10})if(!maps[i].image)return out;
    for(size_t i:{2,3,4,5})if(!source.intrinsic->maps[i])return out;
    const auto& p=*maps[3].image;const uint32_t w=p.width,h=p.height;const size_t count=size_t(w)*h;
    if(w<2||h<2||count>1024*1024){out.report["reason"]="Source shell budget: 2..1048576 analysis pixels; anchor is never resized";return out;}
    for(size_t i:{1,4,5,6,7,8,9,10})if(maps[i].image->width!=w||maps[i].image->height!=h)throw std::runtime_error("Source geometry map size mismatch");
    if(source.shadow->maps[0].width!=w||source.shadow->maps[0].height!=h)throw std::runtime_error("Source geometry shadow size mismatch");
    Mesh mesh;mesh.vertices.resize(count);std::vector<uint8_t> valid(count),covered(count);
    const auto& n=*maps[1].image;const auto& labels=*maps[5].image;
    for(size_t i=0;i<count;++i){auto& v=mesh.vertices[i];
        v.position={p.FloatAt(i,0),p.FloatAt(i,1),p.FloatAt(i,2)};v.normal={n.FloatAt(i,0),n.FloatAt(i,1),n.FloatAt(i,2)};
        const float length=v.normal.x*v.normal.x+v.normal.y*v.normal.y+v.normal.z*v.normal.z;
        const bool excluded=std::find(source.shadow->excludedLabels.begin(),source.shadow->excludedLabels.end(),labels.UintAt(i))!=source.shadow->excludedLabels.end();
        // Bound the float32 shadow fit as well as the input bytes. Extremely small focal lengths
        // can produce finite XYZ that would overflow/cancel in a GPU view/projection matrix.
        const float extent=std::max({std::abs(v.position.x),std::abs(v.position.y),std::abs(v.position.z)});
        valid[i]=!excluded&&extent<=1e6f&&maps[4].image->UintAt(i)!=0&&labels.UintAt(i)!=0&&v.position.z>0&&std::abs(length-1)<.01f;
    }
    auto triangle=[&](uint32_t a,uint32_t b,uint32_t c){
        if(!valid[a]||!valid[b]||!valid[c]||labels.UintAt(a)!=labels.UintAt(b)||labels.UintAt(a)!=labels.UintAt(c))return;
        for(auto [i,j]:{std::pair{a,b},std::pair{b,c},std::pair{c,a}}){const auto& x=mesh.vertices[i].position;const auto& y=mesh.vertices[j].position;
            const float z=std::min(x.z,y.z),distance=std::sqrt((x.x-y.x)*(x.x-y.x)+(x.y-y.y)*(x.y-y.y)+(x.z-y.z)*(x.z-y.z));
            // Reject depth steps and stretched triangles. No bridge across a segmentation boundary.
            if(std::abs(x.z-y.z)>.05f*z||distance>.08f*z)return;
        }
        mesh.indices.insert(mesh.indices.end(),{a,b,c});covered[a]=covered[b]=covered[c]=1;
    };
    for(uint32_t y=0;y+1<h;++y)for(uint32_t x=0;x+1<w;++x){const auto a=y*w+x;triangle(a,a+1,a+w);triangle(a+1,a+w+1,a+w);}
    if(mesh.indices.empty()){out.report["reason"]="No supported source triangles";return out;}
    out.evidence={w,h,NumericFormat::Vector,std::vector<uint8_t>(count*16)};
    float fit=source.lighting->metadata["fit"]["confidence"].get<float>();
    if(source.lighting->metadata["fit"]["backend"]=="manual-test")fit=1;
    size_t receivers=0;
    for(size_t i=0;i<count;++i){
        const float protect=source.shadow->maps[6].FloatAt(i);
        const float old=source.shadow->maps[3].FloatAt(i)*source.shadow->maps[7].FloatAt(i);
        const float error=source.intrinsic->maps[4]->FloatAt(i),unc=source.intrinsic->maps[3]->FloatAt(i,1);
        float support=std::min(maps[9].image->FloatAt(i),maps[10].image->FloatAt(i))*fit*(1-protect);
        const auto& albedo=*maps[6].image;const float minimum=std::min({albedo.FloatAt(i,0),albedo.FloatAt(i,1),albedo.FloatAt(i,2)});
        if(!covered[i]||minimum<.03f||maps[7].image->FloatAt(i)<.2f||maps[8].image->FloatAt(i)>.3f||!source.intrinsic->maps[5]->UintAt(i)||error>.2f||unc>.3f)support=0;
        support=std::clamp(support*std::exp(-error/.15f)*std::exp(-unc/.15f),0.f,1.f);
        const float values[]{source.shadow->maps[1].FloatAt(i),std::clamp(old,0.f,1.f),support,float(covered[i])};
        std::memcpy(out.evidence.bytes.data()+i*16,values,16);receivers+=support>0;
    }
    out.report["triangles"]=mesh.indices.size()/3;out.report["vertices"]=count;out.report["receiverPixels"]=receivers;
    out.report["analysisSize"]={w,h};out.report["relativeDepthEdge"]=.05;out.report["maxEdgePerDepth"]=.08;
    out.report["maxAbsoluteCoordinate"]=1e6;
    out.report["positionProvenance"]=maps[3].metadata;out.report["normalProvenance"]=maps[1].metadata;
    out.report["excludedLabels"]=source.shadow->excludedLabels;
    // Bounds and uploads contain only vertices referenced by accepted triangles, never invalid points.
    std::vector<uint32_t> remap(count);std::vector<Vertex> compact;compact.reserve(count);
    for(size_t i=0;i<count;++i)if(covered[i]){remap[i]=uint32_t(compact.size());compact.push_back(mesh.vertices[i]);}
    for(auto& index:mesh.indices)index=remap[index];mesh.vertices=std::move(compact);out.report["uploadedVertices"]=mesh.vertices.size();
    out.scene.meshes.push_back(std::move(mesh));out.scene.materials.emplace_back();
    Entity entity;entity.name="Immutable source shell";entity.renderer=MeshRenderer{0,0};out.scene.entities.push_back(std::move(entity));out.scene.UpdateWorldMatrices();
    out.available=true;out.report["available"]=true;out.report["reason"]="Source shell supports only observed receivers and occluders";return out;
}
}
