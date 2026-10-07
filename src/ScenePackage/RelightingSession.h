#pragma once
#include "ScenePackage/SourceObservation.h"
#include "Scene/WorkMode.h"
#include "ScenePackage/RelightingParameters.h"
#include "ScenePackage/ImageEditState.h"
#include "ScenePackage/ReferenceAnalysis.h"
#include "ScenePackage/ImageFog.h"
#include "ScenePackage/ImagePointLight.h"
#include <algorithm>
#include <stdexcept>
namespace isr {
struct ImageRect {
    float x{},y{},width{},height{},scale{};
};
inline ImageRect FitSourceImage(uint32_t sw,uint32_t sh,uint32_t vw,uint32_t vh){
    if(!sw||!sh||!vw||!vh)throw std::invalid_argument("Image fit dimensions must be positive");
    const double scale=std::min(double(vw)/double(sw),double(vh)/double(sh));
    const double w=double(sw)*scale,h=double(sh)*scale;
    return {float((double(vw)-w)*.5),float((double(vh)-h)*.5),float(w),float(h),float(scale)};
}
class RelightingSession {
public:
    WorkMode Mode()const{return mode_;}
    bool CanDisplayImage()const{return source_&&source_->CanDisplayImage();}
    bool CanEditPointLights()const{
        if(!CanDisplayImage()||!lighting.available||!source_->analysisMaps)return false;
        const auto& maps=*source_->analysisMaps;
        return maps.maps[1].image&&maps.maps[3].image&&maps.maps[4].image&&maps.metadata.contains("camera")&&maps.metadata["camera"].contains("intrinsicsNormalized");
    }
    bool SetMode(WorkMode mode){if(mode==WorkMode::ImageRelighting&&!CanDisplayImage())return false;if(mode_!=mode)pointEdit.Cancel();mode_=mode;return true;}
    const std::shared_ptr<const SourceObservation>& Source()const{return source_;}
    // Only call at a successful GPU document commit. A failed/cancelled load never reaches this.
    void Publish(std::shared_ptr<const SourceObservation> source)noexcept{
        source_=std::move(source);++revision_;
        lighting.Publish(source_?source_->lighting.get():nullptr);
        relighting={};fog={};
        pointLights.clear();pointEdit={};
        display={};protection={};
        reference={};
        if(!CanDisplayImage())mode_=WorkMode::Scene3D;
    }
    uint64_t Revision()const{return revision_;}
    void RestoreState(RelightingSession&& other)noexcept{const auto revision=revision_;*this=std::move(other);revision_=revision;}
    ImageDebugView imageView=ImageDebugView::Relighted;
    LightingSession lighting;
    RelightingParameters relighting;
    ImageFogParameters fog;
    std::vector<ImagePointLight> pointLights;
    PointLightInteraction pointEdit;
    ImageDisplayState display;
    RegionProtectionEdit protection;
    ReferenceSession reference;
private:
    WorkMode mode_=WorkMode::Scene3D;
    uint64_t revision_=0;
    std::shared_ptr<const SourceObservation> source_;
};
}
