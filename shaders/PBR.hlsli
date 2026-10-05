#ifndef ISR_PBR
#define ISR_PBR
#include "Common.hlsli"
float DistributionGGX(float NoH,float alpha) {
    float a2=alpha*alpha,d=NoH*NoH*(a2-1)+1;
    return a2/(PI*d*d);
}
float GeometrySmithG1(float NoX,float alpha) {
    float a2=alpha*alpha;
    return 2*NoX/max(NoX+sqrt(a2+(1-a2)*NoX*NoX),1e-7);
}
float3 FresnelSchlick(float VoH,float3 f0) {
    float f=pow(1-saturate(VoH),5);return f0+(1-f0)*f;
}
float3 EvaluateBRDF(float3 albedo,float metallic,float roughness,float3 n,float3 v,float3 l) {
    float NoL=saturate(dot(n,l)),NoV=saturate(dot(n,v));
    if(NoL<=0||NoV<=0)return 0;
    float3 h=SafeNormalize(v+l);
    // Perceptual roughness -> microfacet alpha; finite floor prevents singular highlights.
    float alpha=max(roughness,0.045)*max(roughness,0.045);
    float3 F=FresnelSchlick(saturate(dot(v,h)),lerp(0.04.xxx,albedo,metallic));
    float D=DistributionGGX(saturate(dot(n,h)),alpha);
    float G=GeometrySmithG1(NoL,alpha)*GeometrySmithG1(NoV,alpha);
    float3 specular=D*G*F/max(4*NoL*NoV,1e-7);
    float3 diffuse=(1-F)*(1-metallic)*albedo/PI;
    return (diffuse+specular)*NoL;
}
#endif
