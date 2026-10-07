#include "Fullscreen.hlsli"
#include "ColorManagement.hlsli"
Texture2D<float4> values:register(t0);
Texture2D<uint> validity:register(t1);
cbuffer View:register(b0){float4 imageRect;uint2 analysisSize;uint selected;uint available;};
float4 PSMain(float4 p:SV_Position):SV_Target{
    float2 uv=(p.xy-imageRect.xy)/imageRect.zw;
    if(any(uv<0)||any(uv>=1))return float4(.025,.03,.04,1);
    if(!available)return float4(((uint(p.x+p.y)/12)%2)?float3(.35,.08,.35):float3(.1,.04,.1),1);
    uint2 q=min(uint2(uv*analysisSize),analysisSize-1);uint valid=validity.Load(int3(q,0));
    if(selected==5)return float4(float3(valid,valid,valid),1);
    if(!valid)return float4(.2,.03,.2,1);
    float3 value=values.Load(int3(q,0)).rgb;
    if(selected==2)return float4(saturate(.5+.5*value),1); // signed residual; neutral gray=0, never called specular
    if(selected==3)return float4(saturate(value),1); // A/S/R spread as data, no gamma
    if(selected==4)return float4(saturate(value.xxx*4),1); // per-pixel RGB RMS error, display only
    return float4(LinearToSrgb(saturate(value)),1);
}
