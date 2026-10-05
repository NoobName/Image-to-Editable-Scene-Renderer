#include "ColorManagement.hlsli"
#include "Fullscreen.hlsli"
Texture2D<float4> hdrImage:register(t0);
cbuffer Settings:register(b0) { float exposureEV;uint mode;uint toneMapping;float gamma; };
float4 PSMain(float4 position:SV_Position):SV_Target {
    float4 pixel=hdrImage.Load(int3(position.xy,0));float3 value=max(pixel.rgb,0);
    if(pixel.a==0&&mode!=0)return float4((mode==5?1:0).xxx,1);
    if(mode==0)value=DisplayColor(value,exposureEV,toneMapping,gamma);
    else if(mode==1||mode==7||mode==8)value=LinearToSrgb(value);
    // Data views remain raw display values, without exposure or tone mapping.
    return float4(saturate(value),1);
}
