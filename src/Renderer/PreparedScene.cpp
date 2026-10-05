#include "Renderer/PreparedScene.h"
namespace isr {
PreparedScene::PreparedScene(ID3D12Device* device,const Scene& source,std::shared_ptr<const SourceObservation> observed)
    :resources(device,D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,static_cast<UINT>(source.materials.size()*MaterialTextureCount+64),true),
     samplers(device,D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,2048,true),dsv(device,D3D12_DESCRIPTOR_HEAP_TYPE_DSV,1),observation(std::move(observed)) {
    lighting=resources.Allocate(5);
    ComPtr<ID3D12CommandQueue> queue;ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12GraphicsCommandList> list;ComPtr<ID3D12Fence> fence;
    D3D12_COMMAND_QUEUE_DESC desc{};desc.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;
    Check(device->CreateCommandQueue(&desc,IID_PPV_ARGS(&queue)));
    Check(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator)));
    Check(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&list)));
    Check(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)));
    UniqueHandle event(CreateEventW(nullptr,FALSE,FALSE,nullptr));if(!event.Get())Check(HRESULT_FROM_WIN32(GetLastError()));
    scene=std::make_unique<ScenePass>(device,list.Get(),resources,samplers,source);
    shadow=std::make_unique<ShadowPass>(device,dsv,lighting.cpu,source);
    if(observation&&observation->CanDisplayImage())sourceImage=std::make_unique<SourceImagePass>(device,list.Get(),resources,*observation->anchor->pixels);
    if(sourceImage)analysis=std::make_unique<AnalysisTextures>(device,list.Get(),resources,observation->analysisMaps);
    if(sourceImage)lightingPreview=std::make_unique<LightingPreview>(device,list.Get(),resources,observation->lighting);
    Check(list->Close());ID3D12CommandList* lists[]{list.Get()};queue->ExecuteCommandLists(1,lists);
    Check(queue->Signal(fence.Get(),1));Check(fence->SetEventOnCompletion(1,event.Get()));
    const auto wait=WaitForSingleObject(event.Get(),30000);
    if(wait==WAIT_FAILED)Check(HRESULT_FROM_WIN32(GetLastError()));
    if(wait!=WAIT_OBJECT_0){Check(device->GetDeviceRemovedReason());throw std::runtime_error("Background GPU upload timed out");}
    Check(device->GetDeviceRemovedReason());scene->FinishUpload();if(sourceImage)sourceImage->FinishUpload();if(analysis)analysis->FinishUpload();
    if(lightingPreview)lightingPreview->FinishUpload();
}
}
