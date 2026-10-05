#include "Fullscreen.hlsli"
#include "ColorManagement.hlsli"
Texture2D<float4> sourceImage:register(t0);
SamplerState linearClamp:register(s0);
cbuffer ImageView:register(b0) {float4 imageRect;float2 sourceSize;uint grid;float padding;};
float4 PSMain(float4 position:SV_Position):SV_Target {
    // Both spaces use edge origins and half-integer pixel centers. No Scene matrices or material UVs.
    float2 uv=(position.xy-imageRect.xy)/imageRect.zw;
    if(any(uv<0)||any(uv>=1))return float4(0.025,0.03,0.04,1);
    float3 encoded=LinearToSrgb(sourceImage.SampleLevel(linearClamp,uv,0).rgb);
    if(grid!=0){
        float2 pixel=uv*sourceSize;
        float2 distanceToLine=min(fmod(pixel,64),64-fmod(pixel,64))*imageRect.zw/sourceSize;
        if(any(distanceToLine<0.65))encoded=lerp(encoded,float3(0.1,0.85,1),0.8);
    }
    // One hardware sRGB decode, one encode into a UNORM output; no Look, exposure, ACES or Bloom.
    return float4(saturate(encoded),1);
}
