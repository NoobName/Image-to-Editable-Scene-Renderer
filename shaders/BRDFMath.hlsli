#ifndef ISR_BRDF_MATH
#define ISR_BRDF_MATH
float DistributionGGX(float NoH,float alpha) {
    float a2=alpha*alpha,d=NoH*NoH*(a2-1)+1;
    return a2/(3.14159265359*d*d);
}
float GeometrySmithG1(float NoX,float alpha) {
    float a2=alpha*alpha;
    return 2*NoX/max(NoX+sqrt(a2+(1-a2)*NoX*NoX),1e-7);
}
float3 FresnelSchlick(float VoH,float3 f0) {
    float f=pow(1-saturate(VoH),5);return f0+(1-f0)*f;
}
#endif
