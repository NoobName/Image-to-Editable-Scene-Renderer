#include "Fullscreen.hlsli"
#include "ColorManagement.hlsli"
Texture2D<float4> values:register(t0);
Texture2D<uint> fitMask:register(t1);
cbuffer View:register(b0){float4 imageRect;uint2 analysisSize;uint selected;uint available;};
float4 PSMain(float4 position:SV_Position):SV_Target{
    float2 uv=(position.xy-imageRect.xy)/imageRect.zw;
    if(any(uv<0)||any(uv>=1))return float4(.025,.03,.04,1);
    if(!available)return float4(((uint(position.x)+uint(position.y))/12)%2?float3(.35,.08,.35):float3(.1,.04,.1),1);
    // Nearest sampling preserves the support mask and never mixes rejected pixels into the proxy.
    uint2 p=min(uint2(uv*analysisSize),analysisSize-1);uint valid=fitMask.Load(int3(p,0));
    if(selected==3)return float4(float3(valid,valid,valid),1);
    if(!valid&&selected!=1)return float4(.2,.03,.2,1);
    float3 value=values.Load(int3(p,0)).rgb;
    if(selected==2){float r=saturate(value.x);return float4(r,.2*r,1-r,1);}
    // Fixed /4 visualization scale only. This pass never multiplies source RGB or computes target shading.
    return float4(LinearToSrgb(saturate(value/4)),1);
}
