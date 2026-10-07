#include "Renderer/Renderer.h"
#include "Core/Log.h"
#include "Renderer/FrameCapture.h"
#include "Renderer/AnalysisCapture.h"
#include "Renderer/ShadingCapture.h"
#include "Assets/AssetIO.h"
namespace isr {
Renderer::Renderer(HWND window, uint32_t width, uint32_t height, bool warp, Demo demo, const Scene& scene, bool ui,const std::filesystem::path& environment,std::shared_ptr<const SourceObservation> source,std::shared_ptr<const ProtectionMask> protection)
    : context_(warp), rtvHeap_(context_.Device(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, FrameCount+2),
      dsvHeap_(context_.Device(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 8),
      resourceHeap_(context_.Device(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, static_cast<UINT>(scene.materials.size()*MaterialTextureCount+128), true),
      samplerHeap_(context_.Device(), D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 2048, true),
      demo_(demo), width_(width), height_(height) {
    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.Width = width; desc.Height = height; desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1; desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = FrameCount; desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    ComPtr<IDXGISwapChain1> swap;
    Check(context_.Factory()->CreateSwapChainForHwnd(context_.Queue(), window, &desc, nullptr, nullptr, &swap));
    Check(context_.Factory()->MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER));
    Check(swap.As(&swapChain_));
    rtvHeap_.Allocate(FrameCount+2);
    hdrSrv_=resourceHeap_.Allocate();
    lightingViews_=resourceHeap_.Allocate(5);
    dsvHeap_.Allocate();
    CreateTargets();
    if(demo_==Demo::Scene)CreateSceneTargets(width,height);
    for (auto& frame : frames_) frame = std::make_unique<FrameContext>(context_.Device());
    Check(context_.Device()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, frames_[0]->commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList_)));
    if (demo_ == Demo::Triangle) triangle_ = std::make_unique<TrianglePass>(context_.Device(), commandList_.Get());
    if (demo_ == Demo::Scene) scenePass_ = std::make_unique<ScenePass>(context_.Device(), commandList_.Get(), resourceHeap_, samplerHeap_, scene);
    if(scenePass_&&source&&source->CanDisplayImage())sourceImage_=std::make_unique<SourceImagePass>(context_.Device(),commandList_.Get(),resourceHeap_,*source->anchor->pixels);
    if(sourceImage_)analysis_=std::make_unique<AnalysisTextures>(context_.Device(),commandList_.Get(),resourceHeap_,source->analysisMaps);
    if(sourceImage_)lightingPreview_=std::make_unique<LightingPreview>(context_.Device(),commandList_.Get(),resourceHeap_,source->lighting);
    if(sourceImage_)intrinsicPreview_=std::make_unique<IntrinsicPreview>(context_.Device(),commandList_.Get(),resourceHeap_,source->intrinsic);
    if(sourceImage_)shadowPreview_=std::make_unique<ShadowPreview>(context_.Device(),commandList_.Get(),sourceImage_->Image(),source->shadow);
    if(analysis_)imageRelighting_=std::make_unique<ImageRelightingRenderer>(context_.Device(),*analysis_);
    protection_=std::move(protection);
    if(protection_){if(!source||!source->CanDisplayImage())throw std::runtime_error("--protection-mask requires an initial source package");
        protectionSourceId_=source->anchor->metadata["sourceId"].get<std::string>();}
    if(imageRelighting_)imageComposite_=std::make_unique<ImageRelightingComposite>(context_.Device(),commandList_.Get(),*sourceImage_,*imageRelighting_,analysis_.get(),source->lighting.get(),protection_,source->intrinsic.get(),source.get());
    imageTimer_=std::make_unique<ImageGpuTimer>(context_.Device(),context_.Queue());
    Check(commandList_->Close());
    ID3D12CommandList* lists[] = {commandList_.Get()}; context_.Queue()->ExecuteCommandLists(1, lists);
    context_.Flush(); if (triangle_) triangle_->FinishUpload();
    if (scenePass_) scenePass_->FinishUpload();
    if(sourceImage_)sourceImage_->FinishUpload();
    if(analysis_)analysis_->FinishUpload();
    if(lightingPreview_)lightingPreview_->FinishUpload();
    if(intrinsicPreview_)intrinsicPreview_->FinishUpload();
    if(shadowPreview_)shadowPreview_->FinishUpload();
    if(imageComposite_)imageComposite_->FinishUpload();
    if (scenePass_){
        postProcessing_=std::make_unique<PostProcessingPipeline>(context_.Device(),resourceHeap_);
        postProcessing_->Resize(context_.Device(),sceneWidth_,sceneHeight_);
    }
    if(scenePass_){
        shadowPass_=std::make_unique<ShadowPass>(context_.Device(),dsvHeap_,lightingViews_.cpu,scene);
        environment_=std::make_unique<EnvironmentManager>(context_,resourceHeap_,lightingViews_.index+1);
        environment_->Load(environment.empty()?EnvironmentManager::DefaultPath():environment);
        skyPass_=std::make_unique<SkyPass>(context_.Device());
    }
    if (ui && scenePass_){
        inspector_=std::make_unique<LookDevelopmentUI>(window,context_,FrameCount);
        CreateSceneTargets(width,height);
    }
    context_.CheckMessages();
    session_.Publish(std::move(source));
}
Renderer::~Renderer() { if(preparing_.valid())preparing_.wait();try { Finish(); } catch (const std::exception& e) { Log(e.what()); } }
void Renderer::PrepareScene(std::shared_ptr<const ScenePackage> package,std::shared_ptr<const ProtectionMask> recipeMask,bool replaceProtection){
    if(preparing_.valid()||demo_!=Demo::Scene)throw std::runtime_error("A scene upload is already active or scene rendering is disabled");
    ComPtr<ID3D12Device> device=context_.Device();
    // A user mask is associated with its source identity; a different reconstructed image must not inherit it.
    const auto protection=replaceProtection?recipeMask:protection_&&package->observation&&package->observation->anchor&&package->observation->anchor->metadata["sourceId"]==protectionSourceId_?protection_:nullptr;
    preparing_=std::async(std::launch::async,[device,package,protection]{return std::make_unique<PreparedScene>(device.Get(),package->scene,package->observation,protection);});
}
bool Renderer::ScenePrepared()const{return preparing_.valid()&&preparing_.wait_for(std::chrono::seconds(0))==std::future_status::ready;}
void Renderer::CommitPreparedScene(bool discard,RelightingSession* replacement){
    if(!ScenePrepared())throw std::runtime_error("GPU scene is not ready");
    auto prepared=preparing_.get();if(discard)return;
    auto nextProtectionId=prepared->observation&&prepared->observation->anchor?prepared->observation->anchor->metadata["sourceId"].get<std::string>():std::string{};
    environment_->WriteViews(prepared->resources,prepared->lighting.index+1);
    // Retain every resource referenced by previous frames until its graphics fence retires.
    retired_.reserve(retired_.size()+1); // Allocation failure must precede moving any active resource.
    const auto fence=context_.Signal();
    retired_.push_back({fence,std::move(activeScene_),std::move(scenePass_),std::move(shadowPass_),std::move(sourceImage_),std::move(analysis_),std::move(lightingPreview_),std::move(imageRelighting_),std::move(imageComposite_),std::move(intrinsicPreview_),std::move(shadowPreview_)});
    activeScene_=std::move(prepared);session_.Publish(activeScene_->observation);
    if(replacement)session_.RestoreState(std::move(*replacement));
    protection_=activeScene_->importedProtection;protectionSourceId_=std::move(nextProtectionId);
    if(inspector_)inspector_->ResetSelection();
    // Diagnostics must not turn a completed GPU/source publication into a failed CPU scene swap.
    try{
        Log("GPU scene committed without blocking upload on render thread");
        if(session_.CanDisplayImage())Log("Source session committed: "+session_.Source()->anchor->metadata["sourceId"].get<std::string>()+" root="+PathUtf8(session_.Source()->packageRoot));
    }catch(...){OutputDebugStringA("Scene committed; diagnostic output unavailable.\n");}
}
void Renderer::CreateTargets() {
    for (UINT i = 0; i < FrameCount; ++i) {
        Check(swapChain_->GetBuffer(i, IID_PPV_ARGS(&targets_[i])));
        Check(targets_[i]->SetName((L"Back buffer " + std::to_wstring(i)).c_str()));
        context_.Device()->CreateRenderTargetView(targets_[i].Get(), nullptr, rtvHeap_.Cpu(i));
    }
}
void Renderer::CreateSceneTargets(uint32_t width,uint32_t height){
        // Descriptor slots remain stable. All submitted uses must be retired by
        // the caller before replacing these resources or overwriting their views.
        sceneWidth_=width;sceneHeight_=height;
        const D3D12_CLEAR_VALUE clear{DXGI_FORMAT_D32_FLOAT, {.DepthStencil = {1.0f,0}}};
        depth_ = std::make_unique<Texture>(context_.Device(),width,height,DXGI_FORMAT_D32_FLOAT,
            D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,D3D12_RESOURCE_STATE_DEPTH_WRITE,&clear);
        context_.Device()->CreateDepthStencilView(depth_->Resource(),nullptr,dsvHeap_.Cpu(0));
        const D3D12_CLEAR_VALUE hdrClear{EnvironmentFormat,{.Color={0.012f,0.018f,0.028f,0}}};
        hdr_=std::make_unique<Texture>(context_.Device(),width,height,EnvironmentFormat,
            D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,D3D12_RESOURCE_STATE_RENDER_TARGET,&hdrClear);
        context_.Device()->CreateRenderTargetView(hdr_->Resource(),nullptr,rtvHeap_.Cpu(FrameCount));
        D3D12_SHADER_RESOURCE_VIEW_DESC srv{};srv.Format=EnvironmentFormat;srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;
        srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;srv.Texture2D.MipLevels=1;
        context_.Device()->CreateShaderResourceView(hdr_->Resource(),&srv,hdrSrv_.cpu);
        if(postProcessing_)postProcessing_->Resize(context_.Device(),width,height);
        if(inspector_){
            viewportColor_=std::make_unique<Texture>(context_.Device(),width,height,DXGI_FORMAT_R8G8B8A8_UNORM,
                D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            context_.Device()->CreateRenderTargetView(viewportColor_->Resource(),nullptr,rtvHeap_.Cpu(FrameCount+1));
            inspector_->SetViewportTexture(viewportColor_->Resource());
        }
}
void Renderer::Resize(uint32_t width, uint32_t height) {
    if (!width || !height || (width == width_ && height == height_)) return;
    context_.Flush();
    for (auto& target : targets_) target.Reset();
    Check(swapChain_->ResizeBuffers(FrameCount, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 0));
    width_ = width; height_ = height; CreateTargets(); context_.CheckMessages();
    if((scenePass_||activeScene_)&&!inspector_)CreateSceneTargets(width,height);
    Log("Resize: " + std::to_string(width) + "x" + std::to_string(height));
}
void Renderer::UpdateUI(Scene& scene,RenderSettings& settings,InputState& input){
    if(inspector_){
        if(referencePreview_&&referencePreview_->analysis!=session_.reference.analysis){context_.Flush();inspector_->SetReferenceTextures(nullptr,nullptr);referencePreview_.reset();}
        auto* source=activeScene_?activeScene_->sourceImage.get():sourceImage_.get();
        inspector_->SetSourceTexture(source?source->Image().Resource():nullptr);
        inspector_->Update(scene,settings,input,*environment_,session_);
        const auto w=inspector_->ViewportWidth(),h=inspector_->ViewportHeight();
        if(w!=sceneWidth_||h!=sceneHeight_){context_.Flush();CreateSceneTargets(w,h);Log("Viewport resize: "+std::to_string(w)+"x"+std::to_string(h));}
    }
    if((scenePass_||activeScene_)&&session_.Mode()==WorkMode::Scene3D)scene.camera.SetAspect(float(sceneWidth_)/float(sceneHeight_));
    if(environment_){
        if(!settings.environmentPath.empty()&&settings.environmentPath!=environment_->Path()){
            if(environment_->TryLoad(settings.environmentPath)&&activeScene_)environment_->WriteViews(activeScene_->resources,activeScene_->lighting.index+1);
        }
        settings.environmentPath=environment_->Path();
    }
}
bool Renderer::HandleMessage(HWND window,UINT message,WPARAM wp,LPARAM lp){return inspector_&&inspector_->HandleMessage(window,message,wp,lp);}
void Renderer::Render(const Scene& scene, const RenderSettings& settings, bool reverseOrder, const std::filesystem::path& capture) {
    std::erase_if(retired_,[&](const RetiredScene& item){return context_.Completed()>=item.fence;});
    auto* scenePass=activeScene_?activeScene_->scene.get():scenePass_.get();
    auto* shadowPass=activeScene_?activeScene_->shadow.get():shadowPass_.get();
    const UINT index = swapChain_->GetCurrentBackBufferIndex();
    auto& frame = *frames_[index]; frame.Begin(context_);
    imageTimer_->Collect(index);
    Check(commandList_->Reset(frame.commandAllocator.Get(), nullptr));
    if(profiler_)profiler_->Begin(commandList_.Get(),index);
    D3D12_RESOURCE_BARRIER barrier{}; barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = targets_[index].Get(); barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT; barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    commandList_->ResourceBarrier(1, &barrier);
    const auto handle = rtvHeap_.Cpu(index);
    constexpr float clear[] = {0.045f, 0.065f, 0.105f, 1.0f};
    commandList_->ClearRenderTargetView(handle, clear, 0, nullptr);
    const auto dsv = dsvHeap_.Cpu(0);
    if (depth_) commandList_->ClearDepthStencilView(dsv,D3D12_CLEAR_FLAG_DEPTH,1.0f,0,0,nullptr);
    commandList_->OMSetRenderTargets(1, &handle, FALSE, depth_ ? &dsv : nullptr);
    const auto renderWidth=scenePass?sceneWidth_:width_,renderHeight=scenePass?sceneHeight_:height_;
    const D3D12_VIEWPORT viewport{0,0,static_cast<float>(renderWidth),static_cast<float>(renderHeight),0,1};
    const D3D12_RECT scissor{0,0,static_cast<LONG>(renderWidth),static_cast<LONG>(renderHeight)};
    commandList_->RSSetViewports(1,&viewport); commandList_->RSSetScissorRects(1,&scissor);
    if (triangle_) triangle_->Draw(commandList_.Get(), frame);
    ID3D12DescriptorHeap* heaps[]={resourceHeap_.Heap(),samplerHeap_.Heap()};commandList_->SetDescriptorHeaps(2,heaps);
    ID3D12DescriptorHeap* sceneHeaps[]={activeScene_?activeScene_->resources.Heap():resourceHeap_.Heap(),activeScene_?activeScene_->samplers.Heap():samplerHeap_.Heap()};
    std::unique_ptr<FrameCapture> hdrReadback;
    std::unique_ptr<AnalysisCapture> analysisReadback;
    std::unique_ptr<LightingCapture> lightingReadback;
    std::unique_ptr<IntrinsicCapture> intrinsicReadback;
    std::unique_ptr<ShadowCapture> shadowReadback;
    std::unique_ptr<ShadingCapture> shadingReadback;
    const bool imageMode=session_.Mode()==WorkMode::ImageRelighting&&session_.CanDisplayImage();
    if(imageMode){
        auto* relighting=activeScene_?activeScene_->imageRelighting.get():imageRelighting_.get();
        auto* composite=activeScene_?activeScene_->imageComposite.get():imageComposite_.get();
        const auto previousUpdates=composite->Updates();imageTimer_->Begin(commandList_.Get(),index);
        relighting->Update(commandList_.Get(),session_.lighting);
        composite->EditProtection(context_.Device(),commandList_.Get(),frame,*session_.Source(),session_.protection);
        composite->Update(commandList_.Get(),session_.relighting,session_.lighting.cacheValid,&frame,session_.fog);
        imageTimer_->End(commandList_.Get(),index,previousUpdates!=composite->Updates());
        const auto output=inspector_?rtvHeap_.Cpu(FrameCount+1):handle;
        if(viewportColor_)viewportColor_->Transition(commandList_.Get(),D3D12_RESOURCE_STATE_RENDER_TARGET);
        commandList_->SetDescriptorHeaps(2,sceneHeaps);
        const auto* imagePass=activeScene_?activeScene_->sourceImage.get():sourceImage_.get();
        if(!imagePass)throw std::runtime_error("Source session and GPU image were not committed together");
        if(int(session_.imageView)<2)imagePass->Draw(commandList_.Get(),output,sceneWidth_,sceneHeight_,session_.imageView);
        else if(session_.imageView==ImageDebugView::Relighted||(session_.imageView==ImageDebugView::CastFinal&&composite->CastShadow()->Available())){
            composite->Draw(commandList_.Get(),output,sceneWidth_,sceneHeight_,1,session_.display.exposure,session_.display.lowConfidence,session_.display.confidenceThreshold,session_.imageView==ImageDebugView::Relighted);
        }else if(int(session_.imageView)>=74){
            composite->Fog()->Draw(commandList_.Get(),output,sceneWidth_,sceneHeight_,UINT(session_.imageView)-74);
        }else if(int(session_.imageView)>=64){
            composite->CastShadow()->Draw(commandList_.Get(),output,sceneWidth_,sceneHeight_,UINT(session_.imageView)-64);
        }else if(int(session_.imageView)>=55){
            const auto* preview=activeScene_?activeScene_->shadowPreview.get():shadowPreview_.get();const auto& size=session_.Source()->anchor->sourceSize;
            preview->Draw(commandList_.Get(),output,sceneWidth_,sceneHeight_,size[0],size[1],size_t(session_.imageView)-55,session_.lighting.cacheValid);
        }else if(int(session_.imageView)>=44){
            composite->Draw(commandList_.Get(),output,sceneWidth_,sceneHeight_,UINT(session_.imageView)-44+14);
        }else if(int(session_.imageView)>=38){
            const auto* preview=activeScene_?activeScene_->intrinsicPreview.get():intrinsicPreview_.get();const auto& size=session_.Source()->anchor->sourceSize;
            preview->Draw(commandList_.Get(),output,sceneWidth_,sceneHeight_,size[0],size[1],size_t(session_.imageView)-38);
        }else if(int(session_.imageView)>=24){
            composite->Draw(commandList_.Get(),output,sceneWidth_,sceneHeight_,UINT(session_.imageView)-24);
        }else if(int(session_.imageView)>=19){
            const auto& size=session_.Source()->anchor->sourceSize;
            relighting->Draw(commandList_.Get(),output,sceneWidth_,sceneHeight_,size[0],size[1],UINT(session_.imageView)-19);
        }else if(int(session_.imageView)>=15){
            const auto* preview=activeScene_?activeScene_->lightingPreview.get():lightingPreview_.get();
            const auto& size=session_.Source()->anchor->sourceSize;
            preview->Draw(commandList_.Get(),output,sceneWidth_,sceneHeight_,size[0],size[1],size_t(session_.imageView)-15,session_.lighting.cacheValid);
        }else{
            const auto* analysis=activeScene_?activeScene_->analysis.get():analysis_.get();
            const auto& size=session_.Source()->anchor->sourceSize;
            analysis->Draw(commandList_.Get(),output,sceneWidth_,sceneHeight_,size[0],size[1],size_t(session_.imageView)-2);
        }
        if(viewportColor_)viewportColor_->Transition(commandList_.Get(),D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        auto* maps=activeScene_?activeScene_->analysis.get():analysis_.get();
        if(!capture.empty()&&maps&&maps->Data())analysisReadback=std::make_unique<AnalysisCapture>(context_.Device(),commandList_.Get(),*maps);
        auto* lightingMaps=activeScene_?activeScene_->lightingPreview.get():lightingPreview_.get();
        if(!capture.empty()&&lightingMaps&&lightingMaps->Data())lightingReadback=std::make_unique<LightingCapture>(context_.Device(),commandList_.Get(),*lightingMaps);
        auto* intrinsicMaps=activeScene_?activeScene_->intrinsicPreview.get():intrinsicPreview_.get();
        if(!capture.empty()&&intrinsicMaps&&intrinsicMaps->Data())intrinsicReadback=std::make_unique<IntrinsicCapture>(context_.Device(),commandList_.Get(),*intrinsicMaps);
        auto* shadowMaps=activeScene_?activeScene_->shadowPreview.get():shadowPreview_.get();
        if(!capture.empty()&&shadowMaps&&shadowMaps->Data())shadowReadback=std::make_unique<ShadowCapture>(context_.Device(),commandList_.Get(),*shadowMaps);
        if(!capture.empty())shadingReadback=std::make_unique<ShadingCapture>(context_.Device(),commandList_.Get(),*relighting,composite,imageTimer_->Report());
    }else if (scenePass) {
        commandList_->SetDescriptorHeaps(2,sceneHeaps);
        const auto shadow=shadowPass->Draw(commandList_.Get(),frame,scene,scenePass->Assets(),settings);
        commandList_->RSSetViewports(1,&viewport);commandList_->RSSetScissorRects(1,&scissor);
        hdr_->Transition(commandList_.Get(),D3D12_RESOURCE_STATE_RENDER_TARGET);
        const auto hdrRtv=rtvHeap_.Cpu(FrameCount);
        constexpr float background[]={0.012f,0.018f,0.028f,0};
        commandList_->ClearRenderTargetView(hdrRtv,background,0,nullptr);
        commandList_->OMSetRenderTargets(1,&hdrRtv,FALSE,&dsv);
        commandList_->SetDescriptorHeaps(2,heaps);
        skyPass_->Draw(commandList_.Get(),frame,scene.camera,environment_->SkyView(),settings);
        commandList_->SetDescriptorHeaps(2,sceneHeaps);
        scenePass->Draw(commandList_.Get(), frame, scene, settings,shadow,activeScene_?activeScene_->lighting.gpu:lightingViews_.gpu,float(EnvironmentBaker::PrefilterMips-1), reverseOrder);
        if(!capture.empty())hdrReadback=std::make_unique<FrameCapture>(context_.Device(),commandList_.Get(),hdr_->Resource());
        hdr_->Transition(commandList_.Get(),D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        const auto output=inspector_?rtvHeap_.Cpu(FrameCount+1):handle;
        if(viewportColor_)viewportColor_->Transition(commandList_.Get(),D3D12_RESOURCE_STATE_RENDER_TARGET);
        commandList_->SetDescriptorHeaps(2,heaps);
        postProcessing_->Draw(commandList_.Get(),{hdr_.get(),hdrSrv_.gpu},output,settings);
        if(viewportColor_)viewportColor_->Transition(commandList_.Get(),D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    }
    if(inspector_){commandList_->OMSetRenderTargets(1,&handle,FALSE,nullptr);inspector_->Draw(commandList_.Get());}
    if(profiler_)profiler_->End(commandList_.Get(),index,!capture.empty());
    std::unique_ptr<FrameCapture> readback;
    if (!capture.empty()) readback = std::make_unique<FrameCapture>(context_.Device(),commandList_.Get(),targets_[index].Get());
    std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
    commandList_->ResourceBarrier(1, &barrier); Check(commandList_->Close());
    ID3D12CommandList* lists[] = {commandList_.Get()}; context_.Queue()->ExecuteCommandLists(1, lists);
    Check(swapChain_->Present(1, 0)); frame.fenceValue = context_.Signal(); context_.CheckMessages();
    if (readback) { context_.Wait(frame.fenceValue); readback->Save(capture); if(hdrReadback)hdrReadback->LogHdrStatistics();
        if(analysisReadback)analysisReadback->Save(capture);
        if(lightingReadback)lightingReadback->Save(capture);
        if(intrinsicReadback)intrinsicReadback->Save(capture);
    if(shadowReadback)shadowReadback->Save(capture);
        if(shadingReadback)shadingReadback->Save(capture);
        if(imageMode)Log("Image output: RGBA8 UNORM display; independent float targets; no 3D Look pass");context_.CheckMessages(); }
}
void Renderer::Finish() { context_.Flush(); context_.CheckMessages(); }
void Renderer::SaveProfile(const std::filesystem::path& path){
    if(!profiler_)return;
    for(UINT i=0;i<FrameCount;++i)imageTimer_->Collect(i);
    package::Json scene={{"viewport",{sceneWidth_,sceneHeight_}},{"window",{width_,height_}},{"workMode",int(session_.Mode())}};
    if(session_.CanDisplayImage()){scene["source"]=session_.Source()->anchor->sourceSize;scene["sourceId"]=session_.Source()->anchor->metadata["sourceId"];}
    profiler_->Save(path,imageTimer_->Report(),scene);
}
}
