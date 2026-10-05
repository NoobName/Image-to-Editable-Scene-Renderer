#include "EnvironmentMath.hlsli"
TextureCube<float4> sky:register(t0);SamplerState skySampler:register(s0);
cbuffer SkyConstants:register(b0){row_major float4x4 inverseViewProjection;float4 camera,parameters;};
struct SkyPixel {float4 position:SV_Position;float2 clipPosition:TEXCOORD0;};
SkyPixel VSMain(uint id:SV_VertexID){float2 uv=float2((id<<1)&2,id&2);SkyPixel o;o.clipPosition=uv*float2(2,-2)+float2(-1,1);o.position=float4(o.clipPosition,1,1);return o;}
float4 PSMain(SkyPixel input):SV_Target{
    float4 world=mul(float4(input.clipPosition,1,1),inverseViewProjection);float3 ray=normalize(world.xyz/world.w-camera.xyz);
    return float4(sky.SampleLevel(skySampler,RotateEnvironment(ray,parameters.y),0).rgb*parameters.x,1);
}
