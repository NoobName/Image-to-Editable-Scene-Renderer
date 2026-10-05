#include "Fullscreen.hlsli"
#include "ColorManagement.hlsli"
#include "AnalysisSampling.hlsli"
float4 PSRaw(float4 position:SV_Position):SV_Target {
    float2 uv=(position.xy-imageRect.xy)/imageRect.zw;
    if(any(uv<0)||any(uv>=1))return 0;
    return SampleAnalysis(uv);
}
float4 PSMain(float4 position:SV_Position):SV_Target {
    float2 uv=(position.xy-imageRect.xy)/imageRect.zw;
    if(any(uv<0)||any(uv>=1))return float4(.025,.03,.04,1);
    if(available==0)return float4(((uint(position.x+position.y)/12)%2)?float3(.22,.02,.22):float3(.06,.02,.06),1);
    float4 sampled=SampleAnalysis(uv);if(sampled.w==0)return float4(.25,0,.25,1);
    float3 color;
    if(selected==1||selected==2||selected==12)color=sampled.xyz*.5+.5;
    else if(selected==6)color=LinearToSrgb(sampled.xyz);
    else if(selected==3)color=(sampled.xyz-lowValue)/max(highValue-lowValue,1e-6);
    else if(selected==5){uint id=integerValues.Load(int3(clamp(int2(uv*analysisSize),0,int2(analysisSize)-1),0));color=id==0?0:float3(40+(id*73u)%191,40+(id*127u)%191,40+(id*167u)%191)/255.0;}
    else if(selected==0)color=highValue-lowValue<1e-6?.5:(sampled.x-lowValue)/(highValue-lowValue);
    else color=sampled.xxx;
    return float4(saturate(color),1); // Data visualization values; no Look or lighting.
}
