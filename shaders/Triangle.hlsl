cbuffer FrameTint : register(b0) { float4 tint; };
struct VertexInput { float3 position : POSITION; float3 color : COLOR; };
struct PixelInput { float4 position : SV_Position; float3 color : COLOR; };
PixelInput VSMain(VertexInput input) {
    PixelInput output; output.position = float4(input.position, 1); output.color = input.color; return output;
}
float4 PSMain(PixelInput input) : SV_Target { return float4(input.color, 1) * tint; }
