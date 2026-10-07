#ifndef ISR_LIGHTING
#define ISR_LIGHTING
#include "PBR.hlsli"
#include "Material.hlsli"
#include "ShadowSampling.hlsli"
#include "IBL.hlsli"
#include "PointLight.hlsli"
float3 Illuminate(Surface s,float3 position,float3 geometricNormal) {
    float3 v=SafeNormalize(cameraMode.xyz-position);
    // AO affects diffuse ambient only, never direct light or emission.
    float3 result=s.albedo*(1-s.metallic)*ambientCount.x*s.ao+s.emissive;
    result+=EnvironmentLighting(s,v);
    for(uint i=0;i<(uint)ambientCount.y;++i){
        LightData light=lights[i];float attenuation=1;float3 l;
        if(light.positionType.w<0.5)l=SafeNormalize(-light.directionRange.xyz);
        else {
            float3 delta=light.positionType.xyz-position;float d2=max(dot(delta,delta),0.0001);l=delta*rsqrt(d2);
            float range=light.directionRange.w;
            attenuation=PointAttenuation(d2,range);
        }
        float visibility=(int)i==(int)shadowInfo.x?DirectionalVisibility(position,geometricNormal,l):1;
        result+=EvaluateBRDF(s.albedo,s.metallic,s.roughness,s.normal,v,l)*light.colorIntensity.rgb*light.colorIntensity.w*attenuation*visibility;
    }
    return flags.w>0?s.albedo:result;
}
#endif
