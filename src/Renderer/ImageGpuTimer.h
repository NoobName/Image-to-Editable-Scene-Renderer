#pragma once
#include "Renderer/DeviceContext.h"
#include "ScenePackage/JsonSchema.h"
#include <array>
#include <vector>
namespace isr {
// Three frame-indexed timestamp pairs. Collect only after FrameContext::Begin waited its fence.
class ImageGpuTimer {
public:
    ImageGpuTimer(ID3D12Device*,ID3D12CommandQueue*);
    void Collect(UINT);
    void Begin(ID3D12GraphicsCommandList*,UINT);
    void End(ID3D12GraphicsCommandList*,UINT,bool changed);
    package::Json Report()const;
    void SetWarmup(uint64_t frames){warmup_=frames;}
private:
    ComPtr<ID3D12QueryHeap> queries_;ComPtr<ID3D12Resource> readback_;
    UINT64 frequency_=0;std::array<bool,3> pending_{},changed_{};
    double sum_=0,max_=0,last_=0;uint64_t count_=0;
    uint64_t collected_=0,warmup_=0;std::vector<double> samples_;
};
package::Json TimingDistribution(const std::vector<double>& samples);
}
