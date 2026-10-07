#include "Fullscreen.hlsli"
Texture2D<float> values:register(t0);
Texture2D<uint> unknown:register(t1);
Texture2D<float4> source:register(t2);
cbuffer View:register(b0){float4 imageRect;uint2 sourceSize;uint2 analysisSize;uint selected;uint available;uint cacheValid;uint pad;};
float4 PSMain(float4 p:SV_Position):SV_Target{
    float2 uv=(p.xy-imageRect.xy)/imageRect.zw;
    if(any(uv<0)||any(uv>=1))return float4(.025,.03,.04,1);
    if(!available||!cacheValid)return float4(((uint(p.x+p.y)/12)%2)?float3(.35,.08,.35):float3(.1,.04,.1),1);
    uint2 q=min(uint2(uv*analysisSize),analysisSize-1);
    float v=selected==4?float(unknown.Load(int3(q,0))):values.Load(int3(q,0));
    if(selected==8){uint2 s=min(uint2(uv*sourceSize),sourceSize-1);return float4(lerp(source.Load(int3(s,0)).rgb,float3(1,.1,.05),saturate(v)*.45),1);}
    return float4(v,v,v,1);
}
