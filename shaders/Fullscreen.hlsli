#ifndef ISR_FULLSCREEN
#define ISR_FULLSCREEN
float4 VSMain(uint id:SV_VertexID):SV_Position {
    float2 uv=float2((id<<1)&2,id&2);return float4(uv*float2(2,-2)+float2(-1,1),0,1);
}
#endif
