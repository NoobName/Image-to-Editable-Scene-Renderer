#ifndef ISR_COLOR_MANAGEMENT
#define ISR_COLOR_MANAGEMENT
float3 LinearToSrgb(float3 c) {
    c=max(c,0);return select(c<=0.0031308,12.92*c,1.055*pow(c,1.0/2.4)-0.055);
}
float3 AcesApprox(float3 x) {
    // Fitted filmic curve, not the full ACES color pipeline.
    x=min(x,1e10); // Already saturated here; avoid overflow when squaring HDR.
    return saturate((x*(2.51*x+0.03))/(x*(2.43*x+0.59)+0.14));
}
float3 DisplayColor(float3 radiance,float exposureEV,uint toneMapping,float gamma) {
    float3 value=max(radiance,0)*exp2(exposureEV);
    if(toneMapping==1)value=value/(1+value);
    else if(toneMapping==2)value=AcesApprox(value);
    value=pow(saturate(value),1/max(gamma,0.1));
    return LinearToSrgb(value);
}
#endif
