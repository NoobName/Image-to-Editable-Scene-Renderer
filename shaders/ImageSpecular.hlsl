#include "Fullscreen.hlsli"
#include "BRDFMath.hlsli"
Texture2D<float4> normals:register(t0),positions:register(t1),albedo:register(t2);
Texture2D<float> roughness:register(t3),metallic:register(t4);
Texture2D<uint> validity:register(t5);
cbuffer SpecularConstants:register(b0){float4 direction;float4 direct;float scale;float roughnessScale;uint available;uint padding;};
float3 Unit(float3 a){return a*rsqrt(max(dot(a,a),1e-12));}
float4 PSEvaluate(float4 position:SV_Position):SV_Target{
    int3 p=int3(uint2(position.xy),0);if(!available||validity.Load(p)==0)return 0;
    float3 n=Unit(normals.Load(p).xyz),v=Unit(-positions.Load(p).xyz),l=Unit(-direction.xyz);
    float nl=saturate(dot(n,l)),nv=saturate(dot(n,v));if(nl<=0||nv<=0)return float4(0,0,0,1);
    float3 h=Unit(v+l);float r=clamp(roughness.Load(p)*roughnessScale,.045,1),alpha=r*r;
    float3 f=FresnelSchlick(saturate(dot(v,h)),lerp(.04.xxx,albedo.Load(p).rgb,metallic.Load(p)));
    float response=DistributionGGX(saturate(dot(n,h)),alpha)*GeometrySmithG1(nl,alpha)*GeometrySmithG1(nv,alpha)/max(4*nl*nv,1e-7)*nl;
    return float4(f*response*direct.rgb*(3.14159265359*scale),1);
}
