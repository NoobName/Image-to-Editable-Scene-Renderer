#include "Fullscreen.hlsli"
#include "ColorGrading.hlsli"
Texture2D<float4> hdrImage:register(t0);
Texture2D<float4> bloomImage:register(t1);
SamplerState linearClamp:register(s0);
cbuffer Settings:register(b0){
    uint2 imageSize;float exposure;float bloomIntensity;
    float temperature;float tint;float saturation;float contrast;
    float vignetteIntensity;float vignetteRadius;float vignetteSoftness;float padding;
};
float4 PSMain(float4 position:SV_Position):SV_Target {
    float2 uv=position.xy/imageSize;float4 source=hdrImage.Load(int3(position.xy,0));
    float3 color=source.rgb;
    if(bloomIntensity>0)color+=bloomIntensity*bloomImage.SampleLevel(linearClamp,uv,0).rgb;
    color=GradeColor(color,exposure,temperature,tint,saturation,contrast);
    color*=VignetteFactor(uv,float(imageSize.x)/imageSize.y,vignetteIntensity,vignetteRadius,vignetteSoftness);
    return float4(color,source.a);
}
