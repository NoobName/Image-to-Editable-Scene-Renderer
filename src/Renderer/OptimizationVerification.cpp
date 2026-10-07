#include "Renderer/Renderer.h"
#include "Renderer/TextureReadback.h"
#include "Assets/ImageExport.h"
#include "Core/AtomicFile.h"
#include "Core/Log.h"
#include <cmath>
namespace isr {
double Renderer::VerifyOptimizationCandidate(const ReferenceAnalysis& data){
    if(!data.candidate)throw std::runtime_error("Optimization lacks a CPU candidate buffer");
    auto* shading=activeScene_?activeScene_->imageRelighting.get():imageRelighting_.get();if(!shading)throw std::runtime_error("Shading resources unavailable");
    auto candidate=session_.lighting;candidate.target=data.target;candidate.targetGlobalGain=1;
    context_.Flush();auto& frame=*frames_[0];frame.Begin(context_);Check(commandList_->Reset(frame.commandAllocator.Get(),nullptr));
    shading->Update(commandList_.Get(),candidate);auto& texture=*shading->New().Image().texture;
    TextureReadback read(context_.Device(),commandList_.Get(),texture);
    // The copy is ordered before restoring live shading; no unapproved candidate reaches the viewport.
    shading->Update(commandList_.Get(),session_.lighting);
    Check(commandList_->Close());ID3D12CommandList* lists[]{commandList_.Get()};context_.Queue()->ExecuteCommandLists(1,lists);
    frame.fenceValue=context_.Signal();context_.Wait(frame.fenceValue);context_.CheckMessages();const auto actual=read.Read();const auto& expected=*data.candidate;
    if(actual.width!=expected.width||actual.height!=expected.height||actual.format!=expected.format)throw std::runtime_error("Optimization CPU/GPU candidate shape mismatch");
    double error=0;for(size_t p=0;p<size_t(actual.width)*actual.height;++p)for(unsigned c=0;c<4;++c){const double a=actual.FloatAt(p,c),b=expected.FloatAt(p,c);
        if(!std::isfinite(a)||!std::isfinite(b))throw std::runtime_error("Optimization candidate NaN/Inf");error=std::max(error,std::abs(a-b));}
    package::Json report={{"version",1},{"maxAbsoluteError",error},{"tolerance",5e-5},{"passed",error<=5e-5},{"size",{actual.width,actual.height}},
        {"nonfinite",0},{"before",uint32_t(read.Before())},{"capture",2048},{"after",uint32_t(texture.State())},{"liveTargetRestored",true},
        {"scope","actual DX12 unit diffuse New Shading; not a full ratio/specular/shadow CPU approximation"}};
    WriteNumericDds(data.root/"debug/gpu-candidate.dds",actual,true);const auto text=report.dump(2);AtomicWrite(data.root/"gpu-verification.json",std::span(reinterpret_cast<const uint8_t*>(text.data()),text.size()),true);
    Log("Optimization DX12 verification max error="+std::to_string(error)+" tolerance=0.00005; live target restored");
    if(error>5e-5)throw std::runtime_error("Optimization CPU/DX12 mismatch; candidate rejected");return error;
}
}
