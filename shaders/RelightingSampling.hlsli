#ifndef ISR_RELIGHTING_SAMPLING
#define ISR_RELIGHTING_SAMPLING
Texture2D<float4> sourceNormal:register(t5);
Texture2D<float> sourceDepth:register(t6);
Texture2D<uint> sourceLabel:register(t7);
Texture2D<float4> qualityA:register(t8);
Texture2D<float4> qualityB:register(t9);
Texture2D<float> protectionMask:register(t10);
struct ShadingSample {float4 oldValue,newValue,quality;float shadow;float3 intrinsic;};
int2 BoundPixel(int2 p){return clamp(p,0,int2(analysisSize)-1);}
ShadingSample SampleShading(float2 uv){
    int2 center=BoundPixel(int2(floor(uv*analysisSize)));ShadingSample s;
    s.oldValue=oldResponse.Load(int3(center,0));s.newValue=newResponse.Load(int3(center,0));
    s.quality=qualityA.Load(int3(center,0));s.shadow=qualityB.Load(int3(center,0)).x;
    s.intrinsic=qualityB.Load(int3(center,0)).yzw;
    if(stability==0||hasBoundaryMaps==0||all(sourceSize==analysisSize)||s.oldValue.a*s.newValue.a==0)return s;
    // Exact pixel-center correspondences must not leak a protected neighbor through float division noise.
    float2 pixel=uv*analysisSize-.5;float2 closest=round(pixel);
    pixel=float2(abs(pixel.x-closest.x)<1e-4?closest.x:pixel.x,abs(pixel.y-closest.y)<1e-4?closest.y:pixel.y);
    int2 base=int2(floor(pixel));float2 f=frac(pixel);
    float z=sourceDepth.Load(int3(center,0));float3 n=normalize(sourceNormal.Load(int3(center,0)).xyz);
    uint label=sourceLabel.Load(int3(center,0));bool good=true;ShadingSample sum=(ShadingSample)0;
    [unroll]for(int y=0;y<2;++y)[unroll]for(int x=0;x<2;++x){
        int2 p=BoundPixel(base+int2(x,y));float4 a=oldResponse.Load(int3(p,0)),b=newResponse.Load(int3(p,0));float other=sourceDepth.Load(int3(p,0));
        float3 normal=sourceNormal.Load(int3(p,0)).xyz;normal*=rsqrt(max(dot(normal,normal),1e-12));
        good=good&&a.a*b.a!=0&&sourceLabel.Load(int3(p,0))==label&&dot(n,normal)>=normalCosineEdge
            &&abs(z-other)<=relativeDepthEdge*max(min(z,other),1e-6);
        float weight=(x?f.x:1-f.x)*(y?f.y:1-f.y);
        sum.oldValue+=weight*a;sum.newValue+=weight*b;sum.quality+=weight*qualityA.Load(int3(p,0));sum.shadow+=weight*qualityB.Load(int3(p,0)).x;
        sum.intrinsic+=weight*qualityB.Load(int3(p,0)).yzw;
    }
    // All-or-nearest keeps the center's ownership across invalid/depth/normal/label boundaries.
    if(good)return sum;return s;
}
#endif
