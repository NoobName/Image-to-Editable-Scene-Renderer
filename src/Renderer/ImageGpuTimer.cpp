#include "Renderer/ImageGpuTimer.h"
#include <algorithm>
#include <cstring>
#include <cmath>
namespace isr {
ImageGpuTimer::ImageGpuTimer(ID3D12Device* device,ID3D12CommandQueue* queue){
    Check(queue->GetTimestampFrequency(&frequency_));if(!frequency_)throw std::runtime_error("GPU timestamp frequency is zero");
    D3D12_QUERY_HEAP_DESC q{};q.Type=D3D12_QUERY_HEAP_TYPE_TIMESTAMP;q.Count=6;Check(device->CreateQueryHeap(&q,IID_PPV_ARGS(&queries_)));
    D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_READBACK;D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;
    d.Width=6*sizeof(UINT64);d.Height=1;d.DepthOrArraySize=1;d.MipLevels=1;d.SampleDesc.Count=1;d.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    Check(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&d,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&readback_)));
}
void ImageGpuTimer::Collect(UINT frame){
    if(!pending_.at(frame))return;pending_[frame]=false;++collected_;if(!changed_[frame])return;
    void* mapped{};D3D12_RANGE read{frame*2*sizeof(UINT64),(frame*2+2)*sizeof(UINT64)};Check(readback_->Map(0,&read,&mapped));
    UINT64 timestamps[2];std::memcpy(timestamps,static_cast<uint8_t*>(mapped)+read.Begin,sizeof(timestamps));D3D12_RANGE write{0,0};readback_->Unmap(0,&write);
    if(timestamps[1]<timestamps[0])throw std::runtime_error("GPU timestamps are not monotonic");
    last_=double(timestamps[1]-timestamps[0])*1000/double(frequency_);sum_+=last_;max_=std::max(max_,last_);++count_;
    // Bounded storage; no extra queue wait. The owning frame fence has already completed.
    if(collected_>warmup_&&samples_.size()<65536)samples_.push_back(last_);
}
void ImageGpuTimer::Begin(ID3D12GraphicsCommandList* list,UINT frame){list->EndQuery(queries_.Get(),D3D12_QUERY_TYPE_TIMESTAMP,frame*2);}
void ImageGpuTimer::End(ID3D12GraphicsCommandList* list,UINT frame,bool changed){
    list->EndQuery(queries_.Get(),D3D12_QUERY_TYPE_TIMESTAMP,frame*2+1);list->ResolveQueryData(queries_.Get(),D3D12_QUERY_TYPE_TIMESTAMP,frame*2,2,readback_.Get(),frame*2*sizeof(UINT64));
    pending_.at(frame)=true;changed_[frame]=changed;
}
package::Json TimingDistribution(const std::vector<double>& samples){
    if(samples.empty())return {{"count",0},{"available",false}};
    auto sorted=samples;std::sort(sorted.begin(),sorted.end());double sum=0;for(double value:sorted)sum+=value;
    const auto percentile=[&](double p){return sorted[size_t(std::ceil(p*double(sorted.size())))-1];};
    return {{"count",sorted.size()},{"available",true},{"meanMs",sum/double(sorted.size())},{"p50Ms",percentile(.5)},
        {"p95Ms",percentile(.95)},{"maxMs",sorted.back()},{"samplesMs",samples},{"percentile","nearest rank"}};
}
package::Json ImageGpuTimer::Report()const{return {{"changedFrames",count_},{"meanMs",count_?sum_/double(count_):0},{"maxMs",max_},{"lastMs",last_},
    {"warmupFrames",warmup_},{"distribution",TimingDistribution(samples_)},
    {"scope","GPU shading + full-resolution ratio + composition; excludes AI/upload/present/readback/display"}};}
}
