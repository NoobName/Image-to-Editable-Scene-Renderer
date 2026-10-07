#pragma once
#include "Renderer/ImageGpuTimer.h"
#include <filesystem>
namespace isr {
// Optional instrumentation. Whole-frame CPU includes Present/fence pacing; GPU excludes Present.
class FrameProfiler {
public:
    explicit FrameProfiler(DeviceContext&);
    void Begin(ID3D12GraphicsCommandList*,UINT);
    void End(ID3D12GraphicsCommandList*,UINT,bool capture);
    void CpuFrame(double milliseconds,bool capture);
    void Save(const std::filesystem::path&,const package::Json& imageTiming,const package::Json& scene);
private:
    ImageGpuTimer gpu_;
    ComPtr<IDXGIAdapter3> adapter_;
    std::vector<double> cpu_;
    uint64_t frames_=0,peakLocal_=0,peakNonlocal_=0,budget_=0;
    package::Json memorySamples_=package::Json::array();
};
}
