#include "Renderer/ShadingCapture.h"
#include <fstream>
namespace isr {
ShadingCapture::ShadingCapture(ID3D12Device* device,ID3D12GraphicsCommandList* list,ImageRelightingRenderer& renderer,ImageRelightingComposite* composite,package::Json timer):report_(renderer.Report()){
    reads_[0]=std::make_unique<TextureReadback>(device,list,*renderer.Old().Image().texture);
    reads_[1]=std::make_unique<TextureReadback>(device,list,*renderer.New().Image().texture);
    if(composite){report_["composition"]=composite->Report();report_["gpuTiming"]=std::move(timer);
        reads_[2]=std::make_unique<TextureReadback>(device,list,*composite->Ratio().Image().texture);
        reads_[3]=std::make_unique<TextureReadback>(device,list,*composite->Final().Image().texture);
        for(size_t i=0;i<2;++i)reads_[i+4]=std::make_unique<TextureReadback>(device,list,composite->Quality(i));
        for(size_t i=0;i<4;++i)if(auto* image=composite->SpecularTexture(i))reads_[i+6]=std::make_unique<TextureReadback>(device,list,*image);
        if(auto* shadow=composite->CastShadow()){for(size_t i=0;i<6;++i)if(auto* image=shadow->Capture(i))reads_[i+10]=std::make_unique<TextureReadback>(device,list,*image,i<2);}
        reads_[16]=std::make_unique<TextureReadback>(device,list,*composite->Baseline().Image().texture);
        reads_[17]=std::make_unique<TextureReadback>(device,list,composite->Protection());
        if(composite->Fog())reads_[18]=std::make_unique<TextureReadback>(device,list,composite->Fog()->Distance());
        reads_[19]=std::make_unique<TextureReadback>(device,list,*composite->PreFog().Image().texture);}
}
void ShadingCapture::Save(const std::filesystem::path& capture)const{
    const auto directory=capture.parent_path()/(capture.stem().wstring()+L"-shading");std::filesystem::create_directories(directory);
    auto report=report_;
    constexpr const char* names[]{"old","new","ratio","result","qualityA","qualityB","specularOld","specularNew","specularCandidate","specularProtected",
        "castOldMap","castNewMap","castOldVisibility","castNewVisibility","castEvidence","castResidual","baseline26","protection","fogDistance","preFog"};
    for(size_t i=0;i<reads_.size();++i)if(reads_[i]){const auto image=reads_[i]->Read();const char* name=names[i];
        std::ofstream out(directory/(std::string(name)+".bin"),std::ios::binary);out.exceptions(std::ios::badbit|std::ios::failbit);
        out.write(reinterpret_cast<const char*>(image.bytes.data()),image.bytes.size());
        report[name]={{"nonfinite",0},{"size",{image.width,image.height}},{"stateBefore",uint32_t(reads_[i]->Before())},
            {"stateCapture",uint32_t(D3D12_RESOURCE_STATE_COPY_SOURCE)},{"stateAfter",uint32_t(reads_[i]->Before())}};
    }
    std::ofstream out(directory/"readback.json",std::ios::binary);out.exceptions(std::ios::badbit|std::ios::failbit);out<<report.dump(2)<<'\n';
}
}
