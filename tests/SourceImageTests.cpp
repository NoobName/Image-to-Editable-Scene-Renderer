#include "Renderer/SourceImagePass.h"
#include "Renderer/FrameCapture.h"
#include "Renderer/RenderSettings.h"
#include <iostream>
using namespace isr;
int main(){try{
    static_assert(int(RenderMode::OriginalImage)==7&&int(RenderMode::EstimatedRoughness)==10&&int(RenderMode::Final)==0);
    DeviceContext context(true);DescriptorAllocator heap(context.Device(),D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,2,true);
    DescriptorAllocator rtv(context.Device(),D3D12_DESCRIPTOR_HEAP_TYPE_RTV,1);auto output=rtv.Allocate();
    ComPtr<ID3D12CommandAllocator> allocator;ComPtr<ID3D12GraphicsCommandList> list;
    Check(context.Device()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator)));
    Check(context.Device()->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&list)));
    ImageData image;image.width=257;image.height=193;image.rgba.resize(size_t(image.width)*image.height*4);
    for(uint32_t y=0;y<image.height;++y)for(uint32_t x=0;x<image.width;++x){auto* p=image.rgba.data()+(size_t(y)*image.width+x)*4;
        p[0]=uint8_t(x);p[1]=uint8_t((y*17+x*3)%256);p[2]=uint8_t((x^y)%256);p[3]=255;}
    SourceImagePass pass(context.Device(),list.Get(),heap,image);
    auto submit=[&]{Check(list->Close());ID3D12CommandList* lists[]{list.Get()};context.Queue()->ExecuteCommandLists(1,lists);context.Flush();context.CheckMessages();};
    submit();pass.FinishUpload();
    for(unsigned repeat=0;repeat<3;++repeat){
        Check(allocator->Reset());Check(list->Reset(allocator.Get(),nullptr));
        Texture target(context.Device(),image.width,image.height,DXGI_FORMAT_R8G8B8A8_UNORM,D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,D3D12_RESOURCE_STATE_RENDER_TARGET);
        context.Device()->CreateRenderTargetView(target.Resource(),nullptr,output.cpu);ID3D12DescriptorHeap* heaps[]{heap.Heap()};list->SetDescriptorHeaps(1,heaps);
        pass.Draw(list.Get(),output.cpu,image.width,image.height,ImageDebugView::Original);
        FrameCapture capture(context.Device(),list.Get(),target.Resource());submit();
        std::cout<<"Native repeat "<<repeat<<" maxLSB="<<capture.CompareRgb(image,1)<<'\n';
    }
    // Constant image makes the whole letterbox reference exact, including odd viewport sizes.
    image.width=13;image.height=7;image.rgba.resize(13*7*4);
    for(size_t i=0;i<image.rgba.size();i+=4){image.rgba[i]=32;image.rgba[i+1]=128;image.rgba[i+2]=240;image.rgba[i+3]=255;}
    Check(allocator->Reset());Check(list->Reset(allocator.Get(),nullptr));SourceImagePass flat(context.Device(),list.Get(),heap,image);submit();flat.FinishUpload();
    for(const auto size:{std::array<uint32_t,2>{97,23},{23,97},{13,7}}){
        Check(allocator->Reset());Check(list->Reset(allocator.Get(),nullptr));
        Texture target(context.Device(),size[0],size[1],DXGI_FORMAT_R8G8B8A8_UNORM,D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,D3D12_RESOURCE_STATE_RENDER_TARGET);
        context.Device()->CreateRenderTargetView(target.Resource(),nullptr,output.cpu);ID3D12DescriptorHeap* heaps[]{heap.Heap()};list->SetDescriptorHeaps(1,heaps);
        flat.Draw(list.Get(),output.cpu,size[0],size[1],ImageDebugView::Original);FrameCapture capture(context.Device(),list.Get(),target.Resource());submit();
        ImageData reference;reference.width=size[0];reference.height=size[1];reference.rgba.resize(size_t(size[0])*size[1]*4);
        const auto rect=FitSourceImage(13,7,size[0],size[1]);
        for(uint32_t y=0;y<size[1];++y)for(uint32_t x=0;x<size[0];++x){const bool inside=x+.5f>=rect.x&&x+.5f<rect.x+rect.width&&y+.5f>=rect.y&&y+.5f<rect.y+rect.height;
            auto* p=reference.rgba.data()+(size_t(y)*size[0]+x)*4;p[0]=inside?32:6;p[1]=inside?128:8;p[2]=inside?240:10;p[3]=255;}
        std::cout<<"Letterbox "<<size[0]<<'x'<<size[1]<<" maxLSB="<<capture.CompareRgb(reference,1)<<'\n';
    }
    std::cout<<"SourceImage WARP: RGB8 native roundtrip <= 1 LSB; wide/narrow/native letterbox and repeated resources: OK\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
