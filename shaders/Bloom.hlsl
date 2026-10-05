#include "Fullscreen.hlsli"
Texture2D<float4> source:register(t0);
Texture2D<float4> detail:register(t1);
SamplerState linearClamp:register(s0);
cbuffer Settings:register(b0){uint2 outputSize;float2 inverseSourceSize;uint operation;float threshold;float softKnee;float radius;};
float3 Bright(float3 color){
    color=max(color,0);float brightness=max(color.r,max(color.g,color.b));
    float knee=max(threshold*softKnee,0.00001);
    float soft=clamp(brightness-threshold+knee,0,2*knee);soft=soft*soft/(4*knee);
    return color*(max(brightness-threshold,soft)/max(brightness,0.00001));
}
float4 PSMain(float4 position:SV_Position):SV_Target {
    float2 uv=position.xy/outputSize;float3 sum=0;
    if(operation<2){
        // Threshold each source sample before averaging: small highlights survive.
        [unroll]for(int y=0;y<2;++y)[unroll]for(int x=0;x<2;++x){
            float3 c=source.SampleLevel(linearClamp,uv+(float2(x,y)-0.5)*inverseSourceSize,0).rgb;
            sum+=operation==0?Bright(c):c;
        }return float4(sum*0.25,1);
    }
    // Separable 1-2-1 tent kernel (nine taps), normalized to preserve constants.
    [unroll]for(int y=-1;y<=1;++y)[unroll]for(int x=-1;x<=1;++x)
        sum+=source.SampleLevel(linearClamp,uv+float2(x,y)*inverseSourceSize*radius,0).rgb*(x==0?2:1)*(y==0?2:1);
    return float4(0.5*(detail.SampleLevel(linearClamp,uv,0).rgb+sum/16),1);
}
