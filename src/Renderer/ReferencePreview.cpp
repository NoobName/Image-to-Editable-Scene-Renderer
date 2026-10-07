#include "Renderer/Renderer.h"
#include "Assets/AssetIO.h"
#include "Core/Log.h"
namespace isr {
void Renderer::PublishReference(std::shared_ptr<const ReferenceAnalysis> data){
    CheckReferenceSource(*data,session_);
    const double gpuError=!data->optimization.is_null()&&data->canApply?VerifyOptimizationCandidate(*data):0;
    std::unique_ptr<ReferencePreview> candidate;
    if(inspector_){
        // Preview replacements are rare. A bounded flush lets the existing fixed UI slots be
        // rebound safely; no descriptor is allocated on each frame or on each slider edit.
        context_.Flush();auto& frame=*frames_[0];frame.Begin(context_);Check(commandList_->Reset(frame.commandAllocator.Get(),nullptr));
        candidate=std::make_unique<ReferencePreview>(context_.Device(),commandList_.Get(),data);
        Check(commandList_->Close());ID3D12CommandList* lists[]{commandList_.Get()};context_.Queue()->ExecuteCommandLists(1,lists);
        frame.fenceValue=context_.Signal();context_.Wait(frame.fenceValue);for(auto& image:candidate->images)image->FinishUpload();context_.CheckMessages();
        inspector_->SetReferenceTextures(candidate->images[0]->Image().Resource(),candidate->images[1]->Image().Resource());
    }
    referencePreview_=std::move(candidate);session_.reference={};session_.reference.analysis=std::move(data);
    session_.reference.gpuVerified=!session_.reference.analysis->optimization.is_null()&&session_.reference.analysis->canApply;
    session_.reference.gpuError=gpuError;
    Log("Published reference analysis: "+PathUtf8(session_.reference.analysis->root));
}
}
