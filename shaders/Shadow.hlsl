#include "Common.hlsli"
Texture2D baseMap:register(t0);SamplerState baseSampler:register(s0);
PixelInput VSMain(VertexInput v){
    PixelInput o=(PixelInput)0;o.position=mul(float4(v.position,1),worldViewProjection);o.uv0=v.uv0;o.uv1=v.uv1;o.color=v.color;return o;
}
void PSMain(PixelInput input){
    if(flags.x==1)clip(baseMap.Sample(baseSampler,UV(input,0)).a*baseColor.a*input.color.a-emissiveCutoff.w);
}
