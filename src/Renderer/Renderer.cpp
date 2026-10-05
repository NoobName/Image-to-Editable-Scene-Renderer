#include "Renderer/Renderer.h"
#include "Core/Log.h"
#include "Renderer/FrameCapture.h"
namespace isr {
Renderer::Renderer(HWND window, uint32_t width, uint32_t height, bool warp, Demo demo, const Scene& scene, bool ui,const std::filesystem::path& environment)
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
    Check(commandList_->Close());
    ID3D12CommandList* lists[] = {commandList_.Get()}; context_.Queue()->ExecuteCommandLists(1, lists);
    context_.Flush(); if (triangle_) triangle_->FinishUpload();
    if (scenePass_) scenePass_->FinishUpload();
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
}
Renderer::~Renderer() { try { Finish(); } catch (const std::exception& e) { Log(e.what()); } }
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
    if(scenePass_&&!inspector_)CreateSceneTargets(width,height);
    Log("Resize: " + std::to_string(width) + "x" + std::to_string(height));
}
void Renderer::UpdateUI(Scene& scene,RenderSettings& settings,InputState& input){
    if(inspector_){
        inspector_->Update(scene,settings,input,*environment_);
        const auto w=inspector_->ViewportWidth(),h=inspector_->ViewportHeight();
        if(w!=sceneWidth_||h!=sceneHeight_){context_.Flush();CreateSceneTargets(w,h);Log("Viewport resize: "+std::to_string(w)+"x"+std::to_string(h));}
    }
    if(scenePass_)scene.camera.SetAspect(float(sceneWidth_)/float(sceneHeight_));
    if(environment_){
        if(!settings.environmentPath.empty()&&settings.environmentPath!=environment_->Path())environment_->TryLoad(settings.environmentPath);
        settings.environmentPath=environment_->Path();
    }
}
bool Renderer::HandleMessage(HWND window,UINT message,WPARAM wp,LPARAM lp){return inspector_&&inspector_->HandleMessage(window,message,wp,lp);}
void Renderer::Render(const Scene& scene, const RenderSettings& settings, bool reverseOrder, const std::filesystem::path& capture) {
    const UINT index = swapChain_->GetCurrentBackBufferIndex();
    auto& frame = *frames_[index]; frame.Begin(context_);
    Check(commandList_->Reset(frame.commandAllocator.Get(), nullptr));
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
    const auto renderWidth=scenePass_?sceneWidth_:width_,renderHeight=scenePass_?sceneHeight_:height_;
    const D3D12_VIEWPORT viewport{0,0,static_cast<float>(renderWidth),static_cast<float>(renderHeight),0,1};
    const D3D12_RECT scissor{0,0,static_cast<LONG>(renderWidth),static_cast<LONG>(renderHeight)};
    commandList_->RSSetViewports(1,&viewport); commandList_->RSSetScissorRects(1,&scissor);
    if (triangle_) triangle_->Draw(commandList_.Get(), frame);
    ID3D12DescriptorHeap* heaps[]={resourceHeap_.Heap(),samplerHeap_.Heap()};commandList_->SetDescriptorHeaps(2,heaps);
    std::unique_ptr<FrameCapture> hdrReadback;
    if (scenePass_) {
        const auto shadow=shadowPass_->Draw(commandList_.Get(),frame,scene,scenePass_->Assets(),settings);
        commandList_->RSSetViewports(1,&viewport);commandList_->RSSetScissorRects(1,&scissor);
        hdr_->Transition(commandList_.Get(),D3D12_RESOURCE_STATE_RENDER_TARGET);
        const auto hdrRtv=rtvHeap_.Cpu(FrameCount);
        constexpr float background[]={0.012f,0.018f,0.028f,0};
        commandList_->ClearRenderTargetView(hdrRtv,background,0,nullptr);
        commandList_->OMSetRenderTargets(1,&hdrRtv,FALSE,&dsv);
        skyPass_->Draw(commandList_.Get(),frame,scene.camera,environment_->SkyView(),settings);
        scenePass_->Draw(commandList_.Get(), frame, scene, settings,shadow,lightingViews_.gpu,float(EnvironmentBaker::PrefilterMips-1), reverseOrder);
        if(!capture.empty())hdrReadback=std::make_unique<FrameCapture>(context_.Device(),commandList_.Get(),hdr_->Resource());
        hdr_->Transition(commandList_.Get(),D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        const auto output=inspector_?rtvHeap_.Cpu(FrameCount+1):handle;
        if(viewportColor_)viewportColor_->Transition(commandList_.Get(),D3D12_RESOURCE_STATE_RENDER_TARGET);
        postProcessing_->Draw(commandList_.Get(),{hdr_.get(),hdrSrv_.gpu},output,settings);
        if(viewportColor_)viewportColor_->Transition(commandList_.Get(),D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    }
    if(inspector_){commandList_->OMSetRenderTargets(1,&handle,FALSE,nullptr);inspector_->Draw(commandList_.Get());}
    std::unique_ptr<FrameCapture> readback;
    if (!capture.empty()) readback = std::make_unique<FrameCapture>(context_.Device(),commandList_.Get(),targets_[index].Get());
    std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
    commandList_->ResourceBarrier(1, &barrier); Check(commandList_->Close());
    ID3D12CommandList* lists[] = {commandList_.Get()}; context_.Queue()->ExecuteCommandLists(1, lists);
    Check(swapChain_->Present(1, 0)); frame.fenceValue = context_.Signal(); context_.CheckMessages();
    if (readback) { context_.Wait(frame.fenceValue); readback->Save(capture); if(hdrReadback)hdrReadback->LogHdrStatistics();context_.CheckMessages(); }
}
void Renderer::Finish() { context_.Flush(); context_.CheckMessages(); }
}
