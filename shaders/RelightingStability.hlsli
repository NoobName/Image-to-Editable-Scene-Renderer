#ifndef ISR_RELIGHTING_STABILITY
#define ISR_RELIGHTING_STABILITY
#include "RelightingSampling.hlsli"
struct EditResponse {float3 ratio,rawLog,effectiveLog;float4 weights;float signal,shadow,confidence,clamped,protection;};
EditResponse EvaluateEdit(uint2 sourcePixel){
    EditResponse e=(EditResponse)0;e.ratio=1;float2 uv=(sourcePixel+.5)/sourceSize;
    ShadingSample s=SampleShading(uv);float valid=s.oldValue.a*s.newValue.a!=0?1:0;
    float3 originalLinear=OriginalLinear(sourcePixel),encoded=original.Load(int3(sourcePixel,0)).rgb;
    e.weights=saturate(s.quality);e.shadow=saturate(s.shadow);
    if(fitCacheValid==0){e.weights.w=.5;e.shadow=.8;}
    e.signal=smoothstep(.002,.04,dot(originalLinear,luminance))*(1-smoothstep(.92,.98,max(encoded.r,max(encoded.g,encoded.b))));
    uint2 maskPixel=min(uint2(uv*uint2(maskWidth,maskHeight)),uint2(maskWidth,maskHeight)-1);
    e.protection=protectionMask.Load(int3(maskPixel,0));
    float combined=min(min(min(e.weights.x,e.weights.y),min(e.weights.z,e.weights.w)),min(e.signal,e.shadow));
    e.confidence=valid*(1-e.protection)*(stability!=0?combined:1);
    // Independent intrinsic evidence only attenuates the edit; it never supplies the ratio denominator.
    if(padding!=0)e.confidence*=saturate(s.intrinsic.x);
    float3 a=s.oldValue.rgb,b=s.newValue.rgb;
    // These hard invariants bypass log/reciprocal approximations, including exact zero lighting.
    if(valid==0||all(a==b))return e;
    float rawY=log((dot(b,luminance)+epsilon)/(dot(a,luminance)+epsilon));
    float3 chroma=log((b+epsilon)/(a+epsilon))-rawY;
    if(colorMode==0)chroma=0;
    e.rawLog=rawY+chroma;
    float3 limited;
    if(stability!=0){
        float lightSupport=smoothstep(.01,.12,dot(a,luminance));
        float signalSupport=smoothstep(.002,.04,dot(originalLinear,luminance));
        float brighten=lerp(min(1.15,maxRatio),maxRatio,min(lightSupport,signalSupport));
        limited=clamp(rawY,log(minRatio),log(brighten))+clamp(chroma,-log(chromaLimit),log(chromaLimit));
        limited=clamp(limited,log(minRatio),log(maxRatio));
    }else{
        // Stage21 baseline retained as an explicit comparison. Manual protection still applies.
        limited=log(clamp(exp(rawY)*clamp(exp(chroma),.5,2),minRatio,maxRatio));
    }
    e.clamped=any(abs(limited-e.rawLog)>1e-6)?1:0;
    if(strength==0||e.confidence==0)return e;
    e.effectiveLog=limited*(strength*e.confidence);e.ratio=exp(e.effectiveLog);return e;
}
#endif
