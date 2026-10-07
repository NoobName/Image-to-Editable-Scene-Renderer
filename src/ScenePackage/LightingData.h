#pragma once
#include "ScenePackage/AppearanceAnchor.h"
#include "Assets/NumericDds.h"
namespace isr {
inline constexpr std::array<const char*,4> LightingKeys={"proxy","oldShading","residual","fitMask"};
struct LightingParameters {
    std::array<float,3> direction{0,0,1},directColor{1,1,1},ambientColor{1,1,1};
    float directIntensity=1,ambientIntensity=.2f;
    bool operator==(const LightingParameters&)const=default;
};
struct LightingData {
    package::Json metadata;
    LightingParameters source,target;
    std::array<NumericImage,4> maps;
};
std::shared_ptr<const LightingData> LoadLightingData(const std::filesystem::path&,const std::optional<package::AppearanceAnchor>&);
// Mutable calibration belongs to the document session, never the immutable SourceObservation or Scene lights.
struct LightingSession {
    LightingParameters source,target,draft;
    bool available=false,cacheValid=false,manualSource=false;
    uint64_t sourceRevision=0;
    float targetGlobalGain=1; // Session-only control; source calibration and exported observations stay fixed.
    LightingParameters EffectiveTarget()const{auto p=target;p.directIntensity*=targetGlobalGain;p.ambientIntensity*=targetGlobalGain;return p;}
    void Publish(const LightingData* data)noexcept;
    bool ApplySource()noexcept;
};
}
