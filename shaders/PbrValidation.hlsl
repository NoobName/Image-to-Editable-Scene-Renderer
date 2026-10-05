#include "PBR.hlsli"
#include "ColorManagement.hlsli"
RWStructuredBuffer<float4> results:register(u0);
[numthreads(64,1,1)]
void CSMain(uint3 id:SV_DispatchThreadID) {
    uint i=id.x;float3 n=float3(0,0,1),value;
    if(i>=256){
        uint test=i-256;
        if(test<72){
            const float radiance[]={0,0.18,1,16};const float gamma[]={0.5,1,2};
            results[i]=float4(DisplayColor(radiance[test%4].xxx,float((test/4)%2)*2,test/24,gamma[(test/8)%3]),1);
        }else results[i]=0;
        return;
    }
    if(i==0)value=EvaluateBRDF(0.5.xxx,0,1,n,n,n);
    else if(i==1)value=EvaluateBRDF(0.5.xxx,1,1,n,n,n);
    else if(i==2)value=FresnelSchlick(0,0.04.xxx);
    else if(i==3)value=FresnelSchlick(1,0.04.xxx);
    else if(i==4)value=EvaluateBRDF(0.5.xxx,0,0,n,n,-n);
    else {
        float z=float(i%16)/15,roughness=float(i/16)/15;
        value=EvaluateBRDF(float3(0.8,0.3,0.1),float(i%2),roughness,n,SafeNormalize(float3(1,0,0.001)),float3(sqrt(1-z*z),0,z));
    }
    results[i]=float4(value,1);
}
