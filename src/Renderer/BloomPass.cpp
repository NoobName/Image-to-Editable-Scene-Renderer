#include "Renderer/BloomPass.h"
#include "Renderer/ShaderCompiler.h"
#include <algorithm>
namespace isr {
BloomPass::BloomPass(ID3D12Device* device,DescriptorAllocator& resources)
    :rtvHeap_(device,D3D12_DESCRIPTOR_HEAP_TYPE_RTV,MaxLevels*2),
     filter_(device,ExecutableDirectory()/"shaders/Bloom.hlsl",L"PSMain",PostProcessFormat,8){
    for(unsigned i=0;i<MaxLevels;++i){down_[i]=std::make_unique<PostProcessTarget>(rtvHeap_,resources);up_[i]=std::make_unique<PostProcessTarget>(rtvHeap_,resources);}
}
void BloomPass::Resize(ID3D12Device* device,UINT width,UINT height){
    inputWidth_=width;inputHeight_=height;levels_=0;
    do{width=std::max(1u,(width+1)/2);height=std::max(1u,(height+1)/2);
        down_[levels_]->Resize(device,width,height);up_[levels_]->Resize(device,width,height);++levels_;
    }while(levels_<MaxLevels&&(width>1||height>1));
}
PostProcessImage BloomPass::Draw(ID3D12GraphicsCommandList* list,PostProcessImage hdr,const LookParameters& look){
    struct Constants {UINT width,height;float inverseWidth,inverseHeight;UINT operation;float threshold,knee,radius;};
    static_assert(sizeof(Constants)==32);
    auto source=hdr;UINT w=inputWidth_,h=inputHeight_;
    for(unsigned i=0;i<levels_;++i){auto& target=*down_[i];
        const Constants c{target.Width(),target.Height(),1.0f/w,1.0f/h,i==0?0u:1u,look.bloomThreshold,look.bloomSoftKnee,look.bloomRadius};
        target.Begin(list);filter_.Draw(list,target.Rtv(),target.Width(),target.Height(),source.srv,source.srv,&c);target.End(list);
        source=target.Image();w=target.Width();h=target.Height();
    }
    for(int i=int(levels_)-2;i>=0;--i){auto& target=*up_[i];
        const Constants c{target.Width(),target.Height(),1.0f/w,1.0f/h,2,look.bloomThreshold,look.bloomSoftKnee,look.bloomRadius};
        target.Begin(list);filter_.Draw(list,target.Rtv(),target.Width(),target.Height(),source.srv,down_[i]->Image().srv,&c);target.End(list);
        source=target.Image();w=target.Width();h=target.Height();
    }return source;
}
}
