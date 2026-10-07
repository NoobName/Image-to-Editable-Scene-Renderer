#ifndef ISR_POINT_LIGHT
#define ISR_POINT_LIGHT
// Shared finite-range inverse-square falloff. A small denominator floor keeps a
// light coincident with a receiver finite; the cutoff reaches zero continuously.
float PointAttenuation(float distanceSquared,float range){
    float d2=max(distanceSquared,.0001);
    float window=range>0?saturate(1-pow(sqrt(d2)/range,4)):1;
    return window*window/d2;
}
#endif
