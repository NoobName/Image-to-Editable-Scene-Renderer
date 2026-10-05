#include "PBR.hlsli"
#include "EnvironmentMath.hlsli"
Texture2D<float4> panorama:register(t0);
TextureCube<float4> sourceCube:register(t1);
RWTexture2DArray<float4> outputImage:register(u0);
SamplerState panoramaSampler:register(s0);SamplerState cubeSampler:register(s1);
cbuffer BakeParameters:register(b2){uint outputSize;uint bakeMode;float roughness;uint sampleCount;float panoramaLod;float cubeSize;float2 unused;};
float2 IntegrateBRDF(float NoV,float r){
    float3 n=float3(0,0,1),v=float3(sqrt(1-NoV*NoV),0,NoV);float2 sum=0;
    float alpha=max(r,0.045)*max(r,0.045);
    for(uint i=0;i<sampleCount;++i){float3 h=SampleGGX(Hammersley(i,sampleCount),r,n),l=normalize(2*dot(v,h)*h-v);
        float NoL=saturate(l.z),NoH=saturate(h.z),VoH=saturate(dot(v,h));
        if(NoL>0){float g=GeometrySmithG1(NoV,alpha)*GeometrySmithG1(NoL,alpha);
            float visibility=g*VoH/max(NoH*NoV,1e-7),fc=pow(1-VoH,5);sum+=float2(1-fc,fc)*visibility;}
    }return sum/sampleCount;
}
[numthreads(8,8,1)]
void CSMain(uint3 id:SV_DispatchThreadID){
    if(any(id.xy>=outputSize))return;float2 uv=(float2(id.xy)+0.5)/outputSize;
    if(bakeMode==3){outputImage[id]=float4(IntegrateBRDF(uv.x,uv.y),0,1);return;}
    float3 n=CubeDirection(id.z,uv);
    if(bakeMode==0){outputImage[id]=float4(panorama.SampleLevel(panoramaSampler,DirectionUv(n),panoramaLod).rgb,1);return;}
    float3 sum=0;float weight=0;
    if(bakeMode==1){
        for(uint i=0;i<sampleCount;++i){float2 xi=Hammersley(i,sampleCount);float phi=2*PI*xi.x;
            float3 local=float3(cos(phi)*sqrt(xi.y),sin(phi)*sqrt(xi.y),sqrt(1-xi.y));
            float3 l=TangentToWorld(local,n);sum+=sourceCube.SampleLevel(cubeSampler,l,1).rgb;
        }
        outputImage[id]=float4(PI*sum/sampleCount,1);return; // Actual irradiance E, not E/pi.
    }
    if(roughness<0.001){outputImage[id]=sourceCube.SampleLevel(cubeSampler,n,0);return;}
    for(uint i=0;i<sampleCount;++i){float3 h=SampleGGX(Hammersley(i,sampleCount),roughness,n),l=normalize(2*dot(n,h)*h-n);
        float NoL=saturate(dot(n,l));if(NoL>0){
            float NoH=saturate(dot(n,h)),alpha=roughness*roughness;
            float pdf=DistributionGGX(NoH,alpha)*NoH/max(4*NoH,1e-7);
            float sampleSolidAngle=1/max(sampleCount*pdf,1e-7),texelSolidAngle=4*PI/(6*cubeSize*cubeSize);
            float lod=max(0,0.5*log2(sampleSolidAngle/texelSolidAngle));
            sum+=sourceCube.SampleLevel(cubeSampler,l,lod).rgb*NoL;weight+=NoL;
        }
    }outputImage[id]=float4(sum/max(weight,1e-7),1);
}
