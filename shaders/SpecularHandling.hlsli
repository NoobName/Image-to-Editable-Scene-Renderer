#ifndef ISR_SPECULAR_HANDLING
#define ISR_SPECULAR_HANDLING
Texture2D<float4> oldSpecular:register(t11),newSpecular:register(t12),specularCandidate:register(t13),protectedResidual:register(t14);
struct SpecularEdit {float3 candidate,anchor,delta,result,oldValue,newValue,protectedValue;float confidence,clip;};
SpecularEdit EvaluateSpecular(uint2 p,float3 ratio){
    SpecularEdit s=(SpecularEdit)0;float3 originalValue=OriginalLinear(p);s.anchor=originalValue;s.result=originalValue*ratio;
    if(specularAvailable==0)return s;
    uint2 a=min(uint2((p+.5)/sourceSize*analysisSize),analysisSize-1);int3 pixel=int3(a,0);
    float4 candidate=specularCandidate.Load(pixel),residual=protectedResidual.Load(pixel);
    s.oldValue=oldSpecular.Load(pixel).rgb;s.newValue=newSpecular.Load(pixel).rgb;s.confidence=candidate.a;s.protectedValue=residual.rgb;
    if(specularEnabled==0||fitCacheValid==0)return s;
    EditResponse e=EvaluateEdit(p);float amount=specularStrength*strength*(1-e.protection)*e.signal*e.weights.x;
    float3 proposed=candidate.rgb*amount;s.candidate=min(proposed,.6*originalValue);s.anchor=originalValue-s.candidate;
    s.clip=max(residual.a,max(max(proposed.r-s.candidate.r,proposed.g-s.candidate.g),proposed.b-s.candidate.b));
    float3 change=all(s.oldValue==s.newValue)?0:(s.newValue-s.oldValue)*(amount*s.confidence);
    s.delta=clamp(change,-s.candidate,specularMaxDelta*originalValue+.02*amount);
    // Algebraically (I-C)*ratio+C+delta; this form is an exact no-op when ratio=1 and delta=0.
    s.result+=s.candidate*(1-ratio)+s.delta;return s;
}
#endif
