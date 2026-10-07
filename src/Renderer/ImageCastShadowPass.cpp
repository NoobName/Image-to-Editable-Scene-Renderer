#include "Renderer/ImageCastShadowPass.h"
#include "Renderer/SourceImagePass.h"
#include "Renderer/ShaderCompiler.h"
namespace isr {
ImageCastShadowPass::ImageCastShadowPass(ID3D12Device* device,ID3D12GraphicsCommandList* list,Texture& source,PostProcessTarget& baseline,
    const AnalysisTextures& maps,const SourceObservation& observation,NumericTexture& protection)
    :snapshot_(BuildSourceGeometry(observation)),resources_(device,D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,40,true),
    samplers_(device,D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,64,true),dsv_(device,D3D12_DESCRIPTOR_HEAP_TYPE_DSV,2),rtvs_(device,D3D12_DESCRIPTOR_HEAP_TYPE_RTV,3),
    null_(resources_.Allocate()),old_(rtvs_,resources_),next_(rtvs_,resources_),final_(rtvs_,resources_),
    visibilityPass_(device,ExecutableDirectory()/"shaders/ImageShadowVisibility.hlsl",L"PSEvaluate",PostProcessFormat,5,24),
    compositePass_(device,ExecutableDirectory()/"shaders/ImageCastShadow.hlsl",L"PSComposite",PostProcessFormat,11,32),
    previewPass_(device,ExecutableDirectory()/"shaders/ImageCastShadow.hlsl",L"PSPreview",DXGI_FORMAT_R8G8B8A8_UNORM,11,32){
    const auto desc=source.Resource()->GetDesc();sw_=UINT(desc.Width);sh_=desc.Height;
    // A separate final target costs 16 bytes/source pixel. Disabling it never downsizes the source anchor.
    if(uint64_t(sw_)*sh_>16ull*1024*1024){snapshot_.available=false;snapshot_.report["available"]=false;snapshot_.report["reason"]="Cast-shadow output exceeds 256 MiB budget";}
    aw_=snapshot_.evidence.width;ah_=snapshot_.evidence.height;
    old_.Resize(device,aw_,ah_);next_.Resize(device,aw_,ah_);final_.Resize(device,Available()?sw_:1,Available()?sh_:1);
    auto srv=[&](ID3D12Resource* image,DXGI_FORMAT format,D3D12_CPU_DESCRIPTOR_HANDLE handle){D3D12_SHADER_RESOURCE_VIEW_DESC d{};
        d.Format=format;d.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;d.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;d.Texture2D.MipLevels=1;
        device->CreateShaderResourceView(image,&d,handle);};
    srv(nullptr,PostProcessFormat,null_.cpu);
    for(auto& i:inputs_)i=resources_.Allocate();
    srv(source.Resource(),DXGI_FORMAT_R8G8B8A8_UNORM,inputs_[0].cpu);srv(baseline.Image().texture->Resource(),PostProcessFormat,inputs_[1].cpu);
    srv(protection.Image().Resource(),DXGI_FORMAT_R32_FLOAT,inputs_[2].cpu);
    maskWidth_=UINT(protection.Image().Resource()->GetDesc().Width);maskHeight_=protection.Image().Resource()->GetDesc().Height;
    constexpr size_t indices[]{3,1,4};
    for(size_t i=0;i<3;++i){geometryViews_[i]=resources_.Allocate();auto* map=maps.Map(indices[i]);
        srv(map?map->Image().Resource():nullptr,i==2?DXGI_FORMAT_R32_UINT:PostProcessFormat,geometryViews_[i].cpu);}
    evidence_=std::make_unique<NumericTexture>(device,list,resources_,snapshot_.evidence);
    NumericImage zero{1,1,NumericFormat::Vector,std::vector<uint8_t>(16)};
    residual_=std::make_unique<NumericTexture>(device,list,resources_,Available()?*observation.intrinsic->maps[2]:zero);
    if(Available())gpu_=std::make_unique<GpuScene>(device,list,resources_,samplers_,snapshot_.scene);
    for(size_t i=0;i<2;++i){shadowViews_[i]=resources_.Allocate();
        if(Available())shadows_[i]=std::make_unique<ShadowPass>(device,dsv_,shadowViews_[i].cpu,snapshot_.scene);
        else srv(nullptr,DXGI_FORMAT_R32_FLOAT,shadowViews_[i].cpu);}
}
void ImageCastShadowPass::FinishUpload(){evidence_->FinishUpload();residual_->FinishUpload();if(gpu_)gpu_->FinishUpload();}
ImageCastShadowPass::Constants ImageCastShadowPass::Data(UINT view)const{
    Constants c{};c.rect[2]=float(sw_);c.rect[3]=float(sh_);c.sw=sw_;c.sh=sh_;c.aw=aw_;c.ah=ah_;
    c.enabled=Available()&&cacheValid_&&lightingValid_;c.same=source_==target_;c.view=view;
    // Disabling the edit keeps valid raw maps inspectable; only genuinely unavailable data is purple.
    c.epsilon=parameters_.epsilon;c.strength=parameters_.castShadows?parameters_.strength*parameters_.shadowStrength:0;c.maxDelta=parameters_.shadowMaxDelta;
    c.confidence=parameters_.shadowConfidence;c.maskWidth=maskWidth_;c.maskHeight=maskHeight_;
    for(unsigned i=0;i<3;++i){c.sourceDirection[i]=source_.direction[i];c.targetDirection[i]=target_.direction[i];
        c.sourceDirect[i]=source_.directColor[i]*source_.directIntensity;c.sourceAmbient[i]=source_.ambientColor[i]*source_.ambientIntensity;
        c.targetDirect[i]=target_.directColor[i]*target_.directIntensity;}
    return c;
}
std::array<D3D12_GPU_DESCRIPTOR_HANDLE,11> ImageCastShadowPass::Views(bool final)const{return {inputs_[0].gpu,inputs_[1].gpu,old_.Image().srv,next_.Image().srv,
    geometryViews_[1].gpu,evidence_->View(),residual_->View(),inputs_[2].gpu,shadowViews_[0].gpu,shadowViews_[1].gpu,final?(Available()?final_.Image().srv:inputs_[1].gpu):null_.gpu};}
void ImageCastShadowPass::Update(ID3D12GraphicsCommandList* list,FrameContext& frame,const LightingParameters& source,const LightingParameters& target,const RelightingParameters& p,bool cache){
    const auto previous=p==parameters_;parameters_=p;cacheValid_=cache;
    auto valid=[](const LightingParameters& light){float length=0;for(float v:light.direction){if(!std::isfinite(v))return false;length+=v*v;}
        return length>1e-10f&&std::isfinite(length)&&std::isfinite(light.directIntensity)&&light.directIntensity>=0;};
    lightingValid_=valid(source)&&valid(target);
    if(Available()){
        std::array<LightingParameters,2> lights{source,target};std::array<LightingParameters,2> cached{source_,target_};
        for(size_t i=0;i<2;++i){bool dirty=!initialized_||lights[i].direction!=cached[i].direction;
            if(dirty){ID3D12DescriptorHeap* heaps[]{resources_.Heap(),samplers_.Heap()};list->SetDescriptorHeaps(2,heaps);
                const auto& d=valid(lights[i])?lights[i].direction:std::array<float,3>{0,0,1};const DirectX::XMFLOAT3 direction{d[0],d[1],d[2]};
                shadowFrames_[i]=shadows_[i]->Draw(list,frame,snapshot_.scene,*gpu_,RenderSettings{},&direction);++shadowUpdates_[i];}
            if(dirty||!previous){ID3D12DescriptorHeap* heaps[]{resources_.Heap()};list->SetDescriptorHeaps(1,heaps);
                VisibilityConstants c{shadowFrames_[i].viewProjection,{lights[i].direction[0],lights[i].direction[1],lights[i].direction[2]},p.shadowNormalBias,p.shadowBias,UINT(p.shadowPcfRadius),UINT(valid(lights[i])),0};
                std::array<D3D12_GPU_DESCRIPTOR_HANDLE,5> views{geometryViews_[0].gpu,geometryViews_[1].gpu,geometryViews_[2].gpu,evidence_->View(),shadowViews_[i].gpu};
                auto& out=i?next_:old_;out.Begin(list);visibilityPass_.Draw(list,out.Rtv(),aw_,ah_,views,&c);out.End(list);++visibilityUpdates_;}
        }
    }
    source_=source;target_=target;initialized_=true;
    if(Available()){ID3D12DescriptorHeap* heaps[]{resources_.Heap()};list->SetDescriptorHeaps(1,heaps);auto c=Data(0);auto views=Views(false);
        final_.Begin(list);compositePass_.Draw(list,final_.Rtv(),sw_,sh_,views,&c);final_.End(list);}
}
void ImageCastShadowPass::Draw(ID3D12GraphicsCommandList* list,D3D12_CPU_DESCRIPTOR_HANDLE output,UINT w,UINT h,UINT view)const{
    auto c=Data(view);const bool depth=view<2;auto rect=FitSourceImage(depth?1:sw_,depth?1:sh_,w,h);
    c.rect[0]=rect.x;c.rect[1]=rect.y;c.rect[2]=rect.width;c.rect[3]=rect.height;
    ID3D12DescriptorHeap* heaps[]{resources_.Heap()};list->SetDescriptorHeaps(1,heaps);auto views=Views(true);previewPass_.Draw(list,output,w,h,views,&c);
}
Texture* ImageCastShadowPass::Capture(size_t i){if(!Available())return nullptr;if(i<2)return &shadows_[i]->Depth();if(i==2)return old_.Image().texture;
    if(i==3)return next_.Image().texture;if(i==4)return &evidence_->Image();if(i==5)return &residual_->Image();return nullptr;}
package::Json ImageCastShadowPass::Report()const{
    auto report=snapshot_.report;report["oldUpdates"]=shadowUpdates_[0];report["newUpdates"]=shadowUpdates_[1];report["visibilityUpdates"]=visibilityUpdates_;
    report["sourceDirection"]=source_.direction;report["targetDirection"]=target_.direction;report["enabled"]=Data(0).enabled!=0&&parameters_.castShadows;
    report["strength"]=parameters_.shadowStrength;report["confidenceScale"]=parameters_.shadowConfidence;report["maxDeltaPerSource"]=parameters_.shadowMaxDelta;
    report["pcfRadius"]=parameters_.shadowPcfRadius;report["depthBias"]=parameters_.shadowBias;report["normalBias"]=parameters_.shadowNormalBias;
    report["shadowSize"]={ShadowPass::Resolution,ShadowPass::Resolution};
    report["oldViewProjection"]=package::Json::array();report["newViewProjection"]=package::Json::array();
    for(int row=0;row<4;++row)for(int col=0;col<4;++col){report["oldViewProjection"].push_back(shadowFrames_[0].viewProjection.m[row][col]);report["newViewProjection"].push_back(shadowFrames_[1].viewProjection.m[row][col]);}
    report["composition"]="baseline26 + bounded source-diffuse-reflectance * target-direct * NdotL * supported visibility change; positive intrinsic residual protected";
    return report;
}
}
