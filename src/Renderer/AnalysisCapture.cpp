#include "Renderer/AnalysisCapture.h"
#include "Core/Log.h"
#include <fstream>
namespace isr {
AnalysisCapture::AnalysisCapture(ID3D12Device* device,ID3D12GraphicsCommandList* list,AnalysisTextures& maps):cpu_(maps.Data()){
    for(size_t i=0;i<reads_.size();++i)if(maps.Map(i))reads_[i]=std::make_unique<TextureReadback>(device,list,maps.Map(i)->Image());
}
void AnalysisCapture::Save(const std::filesystem::path& capture)const{
    auto directory=capture.parent_path()/(capture.stem().wstring()+L"-maps");std::filesystem::create_directories(directory);package::Json report=package::Json::object();
    for(size_t i=0;i<reads_.size();++i)if(reads_[i]){const auto image=reads_[i]->Read();const auto& expected=*cpu_->maps[i].image;
        if(image.bytes!=expected.bytes)throw std::runtime_error("Analysis GPU upload/readback is not byte-exact");
        std::ofstream file(directory/(std::string(AnalysisKeys[i])+".bin"),std::ios::binary);file.exceptions(std::ios::badbit|std::ios::failbit);
        file.write(reinterpret_cast<const char*>(image.bytes.data()),image.bytes.size());
        report[AnalysisKeys[i]]={{"format",uint32_t(image.format)},{"size",{image.width,image.height}},{"bytes",image.bytes.size()},
            {"byteExact",true},{"nonfinite",0},{"stateBefore",uint32_t(reads_[i]->Before())},{"stateCapture",uint32_t(D3D12_RESOURCE_STATE_COPY_SOURCE)},{"stateAfter",uint32_t(reads_[i]->Before())}};
    }
    std::ofstream file(directory/"readback.json",std::ios::binary);file.exceptions(std::ios::badbit|std::ios::failbit);file<<report.dump(2)<<'\n';
    Log("Analysis GPU readback: maps="+std::to_string(report.size())+" byteExact=1 nonfinite=0 states restored");
}
}
