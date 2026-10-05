#ifndef ISR_COMMON
#define ISR_COMMON
static const float PI=3.14159265359;
float3 SafeNormalize(float3 v) { return v*rsqrt(max(dot(v,v),1e-12)); }
struct UvConstants { float4 offsetScale; float4 rotationSet; };
cbuffer ObjectConstants:register(b0) {
    row_major float4x4 worldViewProjection,world,normalMatrix;
    float4 baseColor,emissiveCutoff,factors,flags,extras;
    UvConstants textureUv[6];
};
struct LightData { float4 positionType,directionRange,colorIntensity; };
cbuffer FrameConstants:register(b1) {
    float4 cameraMode,ambientCount,nearFar;
    LightData lights[8];
    row_major float4x4 lightViewProjection;
    float4 shadowParams,shadowInfo,environment;
};
struct VertexInput { float3 position:POSITION;float3 normal:NORMAL;float4 tangent:TANGENT;float2 uv0:TEXCOORD0;float2 uv1:TEXCOORD1;float4 color:COLOR0; };
struct PixelInput { float4 position:SV_Position;float3 normal:NORMAL;float4 tangent:TANGENT;float2 uv0:TEXCOORD0;float2 uv1:TEXCOORD1;float4 color:COLOR0;float3 worldPosition:TEXCOORD2; };
float2 UV(PixelInput input,uint slot) {
    UvConstants t=textureUv[slot];float2 uv=(t.rotationSet.y>0.5?input.uv1:input.uv0)*t.offsetScale.zw;
    float s=sin(t.rotationSet.x),c=cos(t.rotationSet.x);return float2(c*uv.x-s*uv.y,s*uv.x+c*uv.y)+t.offsetScale.xy;
}
#endif
