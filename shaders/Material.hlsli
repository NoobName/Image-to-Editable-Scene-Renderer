#ifndef ISR_MATERIAL
#define ISR_MATERIAL
#include "Common.hlsli"
Texture2D baseMap:register(t0);Texture2D mrMap:register(t1);Texture2D normalMap:register(t2);
Texture2D emissiveMap:register(t3);Texture2D aoMap:register(t4);
Texture2D originalMap:register(t5);
SamplerState baseSampler:register(s0);SamplerState mrSampler:register(s1);SamplerState normalSampler:register(s2);
SamplerState emissiveSampler:register(s3);SamplerState aoSampler:register(s4);
SamplerState originalSampler:register(s5);
struct Surface { float3 albedo,normal,emissive;float alpha,metallic,roughness,ao; };
Surface ReadSurface(PixelInput input,bool front) {
    Surface s;
    // sRGB SRVs decode base/emissive RGB in hardware; alpha stays linear.
    float4 base=baseMap.Sample(baseSampler,UV(input,0))*baseColor*input.color;
    s.albedo=base.rgb;s.alpha=flags.x==2?base.a:1;
    float4 mr=mrMap.Sample(mrSampler,UV(input,1));
    s.metallic=saturate(factors.x*mr.b);s.roughness=saturate(factors.y*mr.g);
    s.ao=saturate(factors.z)*lerp(1,aoMap.Sample(aoSampler,UV(input,4)).r,saturate(extras.x));
    s.emissive=emissiveMap.Sample(emissiveSampler,UV(input,3)).rgb*emissiveCutoff.xyz;
    float3 n=SafeNormalize(input.normal),t=SafeNormalize(input.tangent.xyz-n*dot(n,input.tangent.xyz));
    float3 b=cross(n,t)*input.tangent.w;
    float2 uv=UV(input,2);
    // UV transforms change the parameterization: use its differential basis.
    if(abs(textureUv[2].rotationSet.x)>1e-6||any(abs(textureUv[2].offsetScale.zw-1)>1e-6)) {
        float3 dx=ddx(input.worldPosition),dy=ddy(input.worldPosition);float2 ux=ddx(uv),uy=ddy(uv);
        float determinant=ux.x*uy.y-ux.y*uy.x;
        if(abs(determinant)>1e-10){
            float3 rawT=(dx*uy.y-dy*ux.y)/determinant,rawB=(dy*ux.x-dx*uy.x)/determinant;
            t=SafeNormalize(rawT-n*dot(n,rawT));b=cross(n,t)*(dot(cross(n,t),rawB)<0?-1:1);
        }
    }
    float3 mapped=normalMap.Sample(normalSampler,uv).xyz*2-1;mapped.xy*=factors.w;
    s.normal=extras.y>0?SafeNormalize(t*mapped.x+b*mapped.y+n*mapped.z):n;
    if(!front&&flags.y>0)s.normal=-s.normal;
    if(flags.x==1)clip(base.a-emissiveCutoff.w);
    return s;
}
#endif
