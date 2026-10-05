#include "Common.hlsli"
#include "Material.hlsli"
#include "Lighting.hlsli"
PixelInput VSMain(VertexInput v) {
    PixelInput o;o.position=mul(float4(v.position,1),worldViewProjection);o.worldPosition=mul(float4(v.position,1),world).xyz;
    o.normal=mul(float4(v.normal,0),normalMatrix).xyz;o.tangent=float4(mul(float4(v.tangent.xyz,0),world).xyz,v.tangent.w*flags.z);
    o.uv0=v.uv0;o.uv1=v.uv1;o.color=v.color;return o;
}
float4 PSMain(PixelInput input,bool front:SV_IsFrontFace):SV_Target {
    Surface s=ReadSurface(input,front);float3 result;
    uint mode=(uint)cameraMode.w;
    if(mode==1)result=s.albedo;
    else if(mode==2)result=s.normal*0.5+0.5;
    else if(mode==3)result=s.roughness.xxx;
    else if(mode==4)result=s.metallic.xxx;
    else if(mode==5){
        float n=nearFar.x,f=nearFar.y;
        float viewDepth=n*f/max(f-input.position.z*(f-n),1e-7);
        result=saturate((viewDepth-n)/(f-n)).xxx;
    }
    else if(mode==6)result=float3(0.15,0.85,0.65); // Unlit wire data view; PSO supplies line rasterization.
    else if(mode==7)result=extras.z>0?originalMap.Sample(originalSampler,UV(input,5)).rgb:float3(1,0,1);
    else if(mode==8)result=baseMap.Sample(baseSampler,UV(input,0)).rgb; // Raw intrinsic map, ignores material edits.
    else if(mode==9)result=normalMap.Sample(normalSampler,UV(input,2)).rgb; // Encoded tangent map, not world normal.
    else if(mode==10)result=mrMap.Sample(mrSampler,UV(input,1)).ggg; // Raw perceptual roughness, no factor.
    else result=Illuminate(s,input.worldPosition,SafeNormalize(input.normal)*(front?1:-1));
    return float4(result,s.alpha);
}
