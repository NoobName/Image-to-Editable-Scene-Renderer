#ifndef ISR_IBL
#define ISR_IBL
#include "EnvironmentMath.hlsli"
TextureCube<float4> irradianceMap:register(t7);
TextureCube<float4> prefilterMap:register(t8);
Texture2D<float4> brdfLut:register(t9);
SamplerState environmentSampler:register(s6);
float3 EnvironmentLighting(Surface s,float3 v){
    if(environment.w<0.5)return 0;
    float NoV=saturate(dot(s.normal,v));float3 f0=lerp(0.04.xxx,s.albedo,s.metallic);
    float3 fresnel=f0+(max((1-s.roughness).xxx,f0)-f0)*pow(1-NoV,5);
    float3 irradiance=irradianceMap.SampleLevel(environmentSampler,RotateEnvironment(s.normal,environment.y),0).rgb;
    float3 reflected=RotateEnvironment(reflect(-v,s.normal),environment.y);
    float3 prefiltered=prefilterMap.SampleLevel(environmentSampler,reflected,s.roughness*environment.z).rgb;
    float2 brdf=brdfLut.SampleLevel(environmentSampler,float2(NoV,s.roughness),0).rg;
    float3 diffuse=(1-fresnel)*(1-s.metallic)*s.albedo*irradiance/PI;
    float3 specular=prefiltered*(f0*brdf.x+brdf.y);
    return (diffuse+specular)*s.ao*environment.x;
}
#endif
