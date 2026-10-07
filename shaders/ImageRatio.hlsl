#include "Fullscreen.hlsli"
#include "ColorManagement.hlsli"
Texture2D<float4> original:register(t0); // UNORM view; explicit sRGB decode exactly once below.
Texture2D<float4> oldResponse:register(t1);
Texture2D<float4> newResponse:register(t2);
Texture2D<float4> ratioImage:register(t3);
Texture2D<float4> relighted:register(t4);
cbuffer RatioConstants:register(b0){float4 imageRect;uint2 sourceSize;uint2 analysisSize;
    float strength;float epsilon;float minRatio;float maxRatio;uint colorMode;uint selected;uint withinBudget;uint padding;
    uint stability;uint hasBoundaryMaps;uint maskWidth;uint maskHeight;
    float chromaLimit;float relativeDepthEdge;float normalCosineEdge;uint fitCacheValid;
    uint specularEnabled;float specularStrength;float specularMaxDelta;uint specularAvailable;
    float displayExposure;uint lowConfidence;float confidenceThreshold;uint reserved;};
static const float3 luminance=float3(.2126,.7152,.0722);
float3 OriginalLinear(uint2 p){float3 c=original.Load(int3(p,0)).rgb;return select(c<=.04045,c/12.92,pow((c+.055)/1.055,2.4));}
#include "RelightingStability.hlsli"
#include "SpecularHandling.hlsli"
float4 PSRatio(float4 position:SV_Position):SV_Target{
    if(!withinBudget)return float4(1,1,1,0);
    EditResponse edit=EvaluateEdit(uint2(position.xy));return float4(edit.ratio,edit.confidence);
}
float4 PSComposite(float4 position:SV_Position):SV_Target{
    if(!withinBudget)return 0;
    uint2 p=uint2(position.xy);return float4(EvaluateSpecular(p,ratioImage.Load(int3(p,0)).rgb).result,1);
}
float4 PSPreview(float4 position:SV_Position):SV_Target{
    if(!withinBudget)return float4(((uint(position.x+position.y)/12)%2)?float3(.45,.12,.4):float3(.14,.08,.16),1);
    float2 uv=(position.xy-imageRect.xy)/imageRect.zw;if(any(uv<0)||any(uv>=1))return float4(6/255.,8/255.,10/255.,1);
    uint2 p=min(uint2(uv*sourceSize),sourceSize-1);float3 rgb=relighted.Load(int3(p,0)).rgb;
    if(selected>=3){
        if(selected>=16){
            if(specularAvailable==0||fitCacheValid==0)return float4(.4,.1,.4,1);
            SpecularEdit s=EvaluateSpecular(p,ratioImage.Load(int3(p,0)).rgb);
            if(selected==16)return float4(LinearToSrgb(s.candidate),1);
            if(selected==17)return float4(s.confidence.xxx,1);
            if(selected==18)return float4(LinearToSrgb(s.anchor),1);
            if(selected==19)return float4(LinearToSrgb(s.oldValue),1);
            if(selected==20)return float4(LinearToSrgb(s.newValue),1);
            if(selected==21)return float4(saturate(.5+s.delta*2),1);
            if(selected==22)return float4(saturate(s.clip*10).xxx,1);
            if(selected==23)return float4(LinearToSrgb(abs(s.result-OriginalLinear(p))),1);
            return float4(saturate(.5+.5*s.protectedValue),1);}
        if(selected>=14){ShadingSample s=SampleShading((p+.5)/sourceSize);
            if(s.intrinsic.z<.5)return float4(.4,.1,.4,1);
            return float4((selected==14?s.intrinsic.x:s.intrinsic.y).xxx,1);}
        EditResponse e=EvaluateEdit(p);float value=0;
        if(selected<=6)value=e.weights[selected-3];
        if(selected==7)value=e.signal;if(selected==8)value=e.shadow;if(selected==9)value=e.confidence;
        if(selected==10)return float4(saturate(.5+e.rawLog/(4*log(2.0))),1);
        if(selected==11)return float4(saturate(.5+e.effectiveLog/(4*log(2.0))),1);
        if(selected==12)value=e.clamped;if(selected==13)value=e.protection;return float4(value.xxx,1);
    }
    if(selected==0)return float4(saturate(.5+log2(max(ratioImage.Load(int3(p,0)).rgb,1e-6))*.25),1);
    if(selected==2)rgb=abs(rgb-OriginalLinear(p));
    // Presentation only: numerical views and cached linear relighting targets stay unchanged.
    if(selected==1){rgb=LinearToSrgb(rgb*exp2(displayExposure));
        if(lowConfidence&&ratioImage.Load(int3(p,0)).a<confidenceThreshold)rgb=lerp(rgb,float3(1,.12,.04),.35);
        return float4(rgb,1);}
    return float4(LinearToSrgb(rgb),1);
}
