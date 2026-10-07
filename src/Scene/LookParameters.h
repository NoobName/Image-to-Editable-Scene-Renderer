#pragma once
#include <array>
#include <cmath>
#include <stdexcept>
#include <string>
#include <string_view>
namespace isr {
enum class ToneMapping { None, Reinhard, ACES };
inline constexpr const char* ToneMappingNames[]={"无","Reinhard","ACES"};

// Renderer-independent, authoritative look state. UI, CLI and a future matcher
// edit this value; GPU constants are transient projections of it, never owners.
struct LookParameters {
    float exposure=0; // EV: linear radiance is multiplied by 2^exposure.
    float temperature=0,tint=0; // Artistic warm/cool and magenta/green balance, not Kelvin.
    float saturation=1,contrast=1;
    float gamma=1; // Creative adjustment; exact sRGB encoding is separate.
    ToneMapping toneMapping=ToneMapping::ACES;
    bool bloomEnabled=false;
    float bloomIntensity=0.08f,bloomThreshold=1,bloomSoftKnee=0.5f,bloomRadius=1;
    float vignetteIntensity=0,vignetteRadius=0.35f,vignetteSoftness=0.65f;
    float fogDensity=0; // Reserved for a future depth-aware fog pass; currently has no visual effect.
};
struct LookParameterInfo {
    std::string_view key,label;
    float LookParameters::* member;
    float minimum,maximum;
    bool active=true;
};
// Stable names and bounds shared by UI, CLI, validation and future optimization.
inline constexpr std::array LookParameterSchema={
    LookParameterInfo{"exposure","Exposure (EV)",&LookParameters::exposure,-16,16},
    LookParameterInfo{"temperature","Temperature",&LookParameters::temperature,-1,1},
    LookParameterInfo{"tint","Tint",&LookParameters::tint,-1,1},
    LookParameterInfo{"saturation","Saturation",&LookParameters::saturation,0,2},
    LookParameterInfo{"contrast","Contrast",&LookParameters::contrast,0.25f,2},
    LookParameterInfo{"gamma","Gamma",&LookParameters::gamma,0.1f,4},
    LookParameterInfo{"bloom-intensity","Intensity",&LookParameters::bloomIntensity,0,2},
    LookParameterInfo{"bloom-threshold","Threshold",&LookParameters::bloomThreshold,0,20},
    LookParameterInfo{"bloom-knee","Soft Knee",&LookParameters::bloomSoftKnee,0,1},
    LookParameterInfo{"bloom-radius","Radius",&LookParameters::bloomRadius,0.5f,2},
    LookParameterInfo{"vignette","Intensity",&LookParameters::vignetteIntensity,0,1},
    LookParameterInfo{"vignette-radius","Radius",&LookParameters::vignetteRadius,0,1},
    LookParameterInfo{"vignette-softness","Softness",&LookParameters::vignetteSoftness,0.01f,1},
    LookParameterInfo{"fog-density","Fog Density (reserved)",&LookParameters::fogDensity,0,1,false}
};
inline void ValidateLookParameters(const LookParameters& look){
    for(const auto& p:LookParameterSchema){const float value=look.*(p.member);
        if(!std::isfinite(value)||value<p.minimum||value>p.maximum)
            throw std::invalid_argument("Look parameter '"+std::string(p.key)+"' must be finite and in ["+
                std::to_string(p.minimum)+", "+std::to_string(p.maximum)+"]");
    }
    if(look.toneMapping<ToneMapping::None||look.toneMapping>ToneMapping::ACES)throw std::invalid_argument("Invalid tone mapping mode");
}
inline bool SetLookParameter(LookParameters& look,std::string_view key,float value){
    for(const auto& p:LookParameterSchema)if(p.key==key&&p.active){
        auto candidate=look;candidate.*(p.member)=value;ValidateLookParameters(candidate);look=candidate;return true;
    }return false;
}
}
