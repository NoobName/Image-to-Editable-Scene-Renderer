#include "Renderer/DescriptorAllocator.h"
#include "Renderer/FrameContext.h"
#include "Renderer/Texture.h"
#include <iostream>
#include <stdexcept>
using namespace isr;
namespace { void Require(bool condition) { if (!condition) throw std::runtime_error("Resource invariant failed"); } }
int main() {
    try {
        DeviceContext context(true);
        for (const auto type : {D3D12_DESCRIPTOR_HEAP_TYPE_RTV,D3D12_DESCRIPTOR_HEAP_TYPE_DSV,D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER}) {
            const bool visible = type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV || type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
            DescriptorAllocator heap(context.Device(),type,2,visible); const auto a = heap.Allocate(), b = heap.Allocate();
            Require(b.cpu.ptr-a.cpu.ptr == context.Device()->GetDescriptorHandleIncrementSize(type));
            Require((a.gpu.ptr != 0) == visible);
            bool exhausted = false; try { heap.Allocate(); } catch (const std::out_of_range&) { exhausted = true; } Require(exhausted);
        }
        FrameContext frame(context.Device()); frame.Begin(context);
        const float constants[4] = {1,2,3,4}; const auto a = frame.constants.Allocate(constants), b = frame.constants.Allocate(constants);
        Require(a%256 == 0 && b-a == 256);
        frame.fenceValue = context.Signal(); frame.Begin(context); Require(frame.constants.Allocate(constants) == a);
        Texture texture(context.Device(),4,4,DXGI_FORMAT_R8G8B8A8_UNORM,D3D12_RESOURCE_FLAG_NONE,D3D12_RESOURCE_STATE_COMMON);
        Require(texture.Resource()->GetDesc().Width == 4);
        context.Flush(); context.CheckMessages(); std::cout << "PASS: descriptor bounds, constant alignment, fence reuse, texture creation\n"; return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
