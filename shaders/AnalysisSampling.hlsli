#ifndef ISR_ANALYSIS_SAMPLING
#define ISR_ANALYSIS_SAMPLING
Texture2D<float4> values:register(t0);
Texture2D<uint> integerValues:register(t1);
Texture2D<uint> geometryValid:register(t2);
Texture2D<float> cameraDepth:register(t3);
Texture2D<uint> labels:register(t4);
cbuffer AnalysisView:register(b0) {
    float4 imageRect; uint2 analysisSize; uint selected; uint available;
    float lowValue; float highValue; float relativeEdge; uint maskKind;
    uint hasDepth; uint hasRegions; uint pad0; uint pad1;
};
int2 ClampPixel(int2 p){return clamp(p,0,int2(analysisSize)-1);}
bool IsValid(int2 p){return maskKind==1?geometryValid.Load(int3(p,0))!=0:maskKind==2?labels.Load(int3(p,0))!=0:true;}
// Discrete labels never interpolate. Continuous footprints must all be valid, in the
// center's region and (for geometry) on the same side of the relative depth edge.
float4 SampleAnalysis(float2 uv){
    if(available==0)return 0;
    int2 center=ClampPixel(int2(floor(uv*analysisSize)));
    if(!IsValid(center))return 0;
    if(selected==4||selected==5)return float4(integerValues.Load(int3(center,0)),0,0,1);
    float4 value=values.Load(int3(center,0));float2 pixel=uv*analysisSize-.5;
    int2 base=int2(floor(pixel));float2 f=frac(pixel);float4 sum=0;bool interpolate=true;
    uint centerLabel=hasRegions!=0?labels.Load(int3(center,0)):0;
    float z=hasDepth!=0?cameraDepth.Load(int3(center,0)):0;
    [unroll]for(int y=0;y<2;++y)[unroll]for(int x=0;x<2;++x){
        int2 p=ClampPixel(base+int2(x,y));
        bool good=IsValid(p)&&(hasRegions==0||labels.Load(int3(p,0))==centerLabel);
        if(maskKind==1&&hasDepth!=0){float other=cameraDepth.Load(int3(p,0));good=good&&abs(other-z)<=relativeEdge*max(min(other,z),1e-6);}
        if(maskKind==1&&hasDepth==0)good=false;
        interpolate=interpolate&&good;
        sum+=values.Load(int3(p,0))*(x?f.x:1-f.x)*(y?f.y:1-f.y);
    }
    if(interpolate)value=sum;
    if(selected==1||selected==2||selected==12){float len=length(value.xyz);if(len<1e-6)return 0;value.xyz/=len;}
    return float4(value.xyz,1); // W reports sampling validity, never an invented position component.
}
#endif
