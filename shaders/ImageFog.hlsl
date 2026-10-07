#include "Fullscreen.hlsli"
#include "ColorManagement.hlsli"
Texture2D<float4> baseline:register(t0);
Texture2D<float4> distanceMap:register(t1); // ray distance, geometry confidence, camera Z, validity
Texture2D<uint> labels:register(t2);
Texture2D<float> protection:register(t3);
cbuffer FogConstants:register(b0){float4 imageRect;uint2 sourceSize;uint2 analysisSize;
    float density;float3 airlight;uint active;uint selected;float maxDistance;float padding;};
float4 SampleDistance(uint2 pixel){
    float2 coord=(pixel+.5)*analysisSize/sourceSize-.5;int2 nearest=clamp(int2(floor(coord+.5)),0,int2(analysisSize)-1);
    float4 center=distanceMap.Load(int3(nearest,0));if(center.w==0)return 0;
    uint label=labels.Load(int3(nearest,0));float2 f=frac(coord);int2 base=int2(floor(coord));float4 result=0;
    [unroll]for(int y=0;y<2;++y)[unroll]for(int x=0;x<2;++x){int2 q=clamp(base+int2(x,y),0,int2(analysisSize)-1);
        float4 s=distanceMap.Load(int3(q,0));
        // Never mix invalid/sky/other-object or discontinuous geometry into a valid receiver.
        if(s.w==0||labels.Load(int3(q,0))!=label||abs(s.z-center.z)>.05*min(s.z,center.z))return center;
        result+=s*(x?f.x:1-f.x)*(y?f.y:1-f.y);}
    return result;
}
float3 FogValues(uint2 p){float4 s=SampleDistance(p);float confidence=active?saturate(s.y)*(1-saturate(protection.Load(int3(p,0)))):0;
    float t=confidence>0?exp(-min(density*s.x,80.0)):1;
    return float3(s.x,confidence,1-confidence*(1-t));}
float4 PSComposite(float4 position:SV_Position):SV_Target{
    uint2 p=uint2(position.xy);float4 c=baseline.Load(int3(p,0));float t=FogValues(p).z;
    // Exact no-op branch. Airlight is a fixed display-linear color, not another ambient light/exposure.
    if(t==1)return c;return float4(c.rgb*t+airlight*(1-t),c.a);
}
float4 PSPreview(float4 position:SV_Position):SV_Target{
    float2 uv=(position.xy-imageRect.xy)/imageRect.zw;if(any(uv<0)||any(uv>=1))return float4(6/255.,8/255.,10/255.,1);
    uint2 p=min(uint2(uv*sourceSize),sourceSize-1);float3 s=FogValues(p);float4 d=SampleDistance(p);
    if(selected==0)return d.w>0?float4(saturate(s.x/max(maxDistance,1e-6)).xxx,1):float4(.4,.1,.4,1);
    if(selected==1)return float4(s.z.xxx,1);
    if(selected==2)return float4(s.y.xxx,1);
    return float4(airlight*(1-s.z),1); // Raw linear numeric preview; no creative Look/exposure/encode.
}
