#include "Fullscreen.hlsli"
#include "ColorManagement.hlsli"
Texture2D<float4> geometryNormal:register(t0);
Texture2D<uint> geometryValidity:register(t1);
Texture2D<float4> oldShading:register(t2);
Texture2D<float4> newShading:register(t3);
cbuffer ImageShadingConstants:register(b0){
    float4 imageRect;uint2 analysisSize;uint selected;uint available;
    float4 travel;float4 directRGB;float4 ambientRGB;
};
bool NormalAt(uint2 p,out float3 n){
    n=geometryNormal.Load(int3(p,0)).xyz;float q=dot(n,n);
    bool valid=geometryValidity.Load(int3(p,0))!=0&&all(isfinite(n))&&abs(q-1)<.011;
    n=valid?n*rsqrt(max(q,1e-12)):0;return valid;
}
float LightDot(float3 n){return dot(n,-travel.xyz*rsqrt(max(dot(travel.xyz,travel.xyz),1e-12)));}
float4 PSEvaluate(float4 position:SV_Position):SV_Target{
    float3 n;if(!available||!NormalAt(uint2(position.xy),n))return 0;
    // Unit-reflectance response in the fitted relative gauge. pi is already absorbed in the coefficients.
    return float4(max(LightDot(n),0)*directRGB.xyz+ambientRGB.xyz,1);
}
float4 PSPreview(float4 position:SV_Position):SV_Target{
    float2 uv=(position.xy-imageRect.xy)/imageRect.zw;
    if(any(uv<0)||any(uv>=1))return float4(.035,.045,.055,1);
    if(!available)return float4(((uint(position.x+position.y)/12)%2)?float3(.45,.12,.4):float3(.14,.08,.16),1);
    uint2 p=min(uint2(uv*analysisSize),analysisSize-1);float4 a=oldShading.Load(int3(p,0)),b=newShading.Load(int3(p,0));
    if(selected==4)return float4((a.a*b.a).xxx,1);
    if(a.a*b.a==0)return float4(.45,.08,.4,1);
    float3 result=0;
    if(selected<2)result=LinearToSrgb(max(selected==0?a.rgb:b.rgb,0)/4);
    if(selected==2)result=LinearToSrgb(abs(b.rgb-a.rgb)/4);
    if(selected==3){float3 n;NormalAt(p,n);float v=LightDot(n);result=v>=0?float3(v,v,v):float3(-v,0,0);}
    return float4(result,1);
}
