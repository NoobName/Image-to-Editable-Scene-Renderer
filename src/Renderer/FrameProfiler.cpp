#include "Renderer/FrameProfiler.h"
#include "Core/AtomicFile.h"
namespace isr {
FrameProfiler::FrameProfiler(DeviceContext& context):gpu_(context.Device(),context.Queue()){
    gpu_.SetWarmup(60);
    Check(context.Factory()->EnumAdapterByLuid(context.Device()->GetAdapterLuid(),IID_PPV_ARGS(&adapter_)));
}
void FrameProfiler::Begin(ID3D12GraphicsCommandList* list,UINT index){gpu_.Collect(index);gpu_.Begin(list,index);}
void FrameProfiler::End(ID3D12GraphicsCommandList* list,UINT index,bool capture){gpu_.End(list,index,!capture);}
void FrameProfiler::CpuFrame(double milliseconds,bool capture){
    ++frames_;if(frames_>60&&!capture&&cpu_.size()<65536)cpu_.push_back(milliseconds);
    DXGI_QUERY_VIDEO_MEMORY_INFO local{},nonlocal{};
    Check(adapter_->QueryVideoMemoryInfo(0,DXGI_MEMORY_SEGMENT_GROUP_LOCAL,&local));
    Check(adapter_->QueryVideoMemoryInfo(0,DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL,&nonlocal));
    peakLocal_=std::max(peakLocal_,local.CurrentUsage);peakNonlocal_=std::max(peakNonlocal_,nonlocal.CurrentUsage);budget_=local.Budget;
    if(frames_%30==0&&memorySamples_.size()<4096)memorySamples_.push_back({{"frame",frames_},{"localBytes",local.CurrentUsage},{"nonlocalBytes",nonlocal.CurrentUsage}});
}
void FrameProfiler::Save(const std::filesystem::path& path,const package::Json& imageTiming,const package::Json& scene){
    // Finish/Flush precedes this call, including the last three ring slots.
    for(UINT i=0;i<3;++i)gpu_.Collect(i);
    auto gpu=gpu_.Report();gpu["scope"]="graphics command list, excluding Present; capture frames excluded";
    package::Json report={{"version",1},{"warmupFrames",60},{"sampleLimit",65536},{"cpuFrame",TimingDistribution(cpu_)},
        {"cpuScope","loop after message pump through Present; includes UI/workflows/fence pacing, excludes capture/export/startup"},
        {"gpuFrame",gpu},{"imageUpdate",imageTiming},{"scene",scene},
        {"memory",{{"peakLocalBytes",peakLocal_},{"peakNonlocalBytes",peakNonlocal_},{"localBudgetBytes",budget_},{"samples",memorySamples_},
        {"scope","DXGI per-process WDDM usage sampled at frame ends, includes allocations/residency overhead; not CUDA allocator peak or adapter-wide VRAM"}}}};
    const auto data=report.dump(2);AtomicWrite(path,{reinterpret_cast<const uint8_t*>(data.data()),data.size()});
}
}
