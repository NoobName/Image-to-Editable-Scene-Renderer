#include "Renderer/TrianglePass.h"
#include "Renderer/ShaderCompiler.h"
namespace isr {
TrianglePass::TrianglePass(ID3D12Device* device, ID3D12GraphicsCommandList* list) {
    D3D12_ROOT_PARAMETER parameter{}; parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    parameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    root_ = std::make_unique<RootSignature>(device, std::span(&parameter, 1), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
    ShaderCompiler compiler;
#ifdef _DEBUG
    constexpr bool debug = true;
#else
    constexpr bool debug = false;
#endif
    const auto file = ExecutableDirectory() / "shaders/Triangle.hlsl";
    auto vs = compiler.Compile(file, L"VSMain", ShaderStage::Vertex, debug);
    auto ps = compiler.Compile(file, L"PSMain", ShaderStage::Pixel, debug);
    const D3D12_INPUT_ELEMENT_DESC layout[] = {
        {"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
        {"COLOR",0,DXGI_FORMAT_R32G32B32_FLOAT,0,12,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0}};
    auto desc = PipelineState::GraphicsDefaults(); desc.pRootSignature = root_->Get();
    desc.VS = {vs->GetBufferPointer(),vs->GetBufferSize()}; desc.PS = {ps->GetBufferPointer(),ps->GetBufferSize()};
    desc.InputLayout = {layout, 2};
    pipeline_ = std::make_unique<PipelineState>(device, desc);
    struct Vertex { float position[3], color[3]; };
    const Vertex vertices[] = {{{0,0.65f,0},{1,0.25f,0.2f}},{{0.65f,-0.65f,0},{0.25f,1,0.4f}},{{-0.65f,-0.65f,0},{0.2f,0.4f,1}}};
    vertices_ = std::make_unique<GpuBuffer>(device,list,std::as_bytes(std::span(vertices)),D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
    // Exercise the compute compiler and PSO path without adding compute rendering.
    RootSignature computeRoot(device, {}, D3D12_ROOT_SIGNATURE_FLAG_NONE);
    auto cs = compiler.Compile(ExecutableDirectory() / "shaders/ValidationCompute.hlsl", L"CSMain", ShaderStage::Compute, debug);
    D3D12_COMPUTE_PIPELINE_STATE_DESC compute{}; compute.pRootSignature = computeRoot.Get();
    compute.CS = {cs->GetBufferPointer(),cs->GetBufferSize()}; PipelineState computePipeline(device, compute);
}
void TrianglePass::Draw(ID3D12GraphicsCommandList* list, FrameContext& frame) {
    list->SetPipelineState(pipeline_->Get()); list->SetGraphicsRootSignature(root_->Get());
    const float tint[] = {1,1,1,1}; list->SetGraphicsRootConstantBufferView(0, frame.constants.Allocate(tint));
    const auto view = vertices_->VertexView(6 * sizeof(float)); list->IASetVertexBuffers(0,1,&view);
    list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST); list->DrawInstanced(3,1,0,0);
}
}
