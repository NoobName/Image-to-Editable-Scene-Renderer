#ifndef ISR_COLOR_GRADING
#define ISR_COLOR_GRADING
static const float3 GradingLuma=float3(0.2126,0.7152,0.0722);
float3 BalanceColor(float3 color,float temperature,float tint){
    // Artistic RGB balance, not a calibrated CCT/Bradford white adaptation.
    float3 gains=exp2(float3(0.5*temperature+0.25*tint,-0.5*tint,-0.5*temperature+0.25*tint));
    gains/=dot(gains,GradingLuma); // Neutral gray keeps its luminance.
    return color*gains;
}
float3 GradeColor(float3 color,float exposure,float temperature,float tint,float saturation,float contrast){
    color=BalanceColor(max(color,0)*exp2(exposure),temperature,tint);
    float luminance=dot(color,GradingLuma);
    color=max(lerp(luminance.xxx,color,saturation),0);
    // Contrast about 18% scene-linear gray, before tone mapping. Keep zero black.
    if(contrast!=1)color=0.18*pow(min(color,1e10)/0.18,contrast);
    return color;
}
float VignetteFactor(float2 uv,float aspect,float intensity,float radius,float softness){
    float2 p=(uv*2-1)*float2(aspect,1);
    float distance=length(p)/length(float2(aspect,1)); // Circular in pixel space; corners have d=1.
    return 1-intensity*smoothstep(radius,radius+softness,distance);
}
#endif
