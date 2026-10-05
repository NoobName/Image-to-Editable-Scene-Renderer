#include "Renderer/LightingPreview.h"
#include "Renderer/ShaderCompiler.h"
#include "Core/Log.h"
#include <fstream>
namespace isr {
LightingPreview::LightingPreview(ID3D12Device* device,ID3D12GraphicsCommandList* list,DescriptorAllocator& heap,std::shared_ptr<const LightingData> data)
    :data_(std::move(data)),pass_(device,ExecutableDirectory()/"shaders/LightingPreview.hlsl",L"PSMain",DXGI_FORMAT_R8G8B8A8_UNORM,8){
    if(data_)for(size_t i=0;i<4;++i)maps_[i]=std::make_unique<NumericTexture>(device,list,heap,data_->maps[i]);
    nullFloat_=heap.Allocate();nullUint_=heap.Allocate();
    D3D12_SHADER_RESOURCE_VIEW_DESC desc{};desc.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;
    desc.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;desc.Texture2D.MipLevels=1;
    desc.Format=DXGI_FORMAT_R32G32B32A32_FLOAT;device->CreateShaderResourceView(nullptr,&desc,nullFloat_.cpu);
    desc.Format=DXGI_FORMAT_R32_UINT;device->CreateShaderResourceView(nullptr,&desc,nullUint_.cpu);
}
void LightingPreview::FinishUpload(){for(auto& map:maps_)if(map)map->FinishUpload();}
void LightingPreview::Draw(ID3D12GraphicsCommandList* list,D3D12_CPU_DESCRIPTOR_HANDLE output,uint32_t vw,uint32_t vh,uint32_t sw,uint32_t sh,size_t selected,bool cacheValid)const{
    if(selected>=4)throw std::out_of_range("Unknown lighting preview");
    const auto rect=FitSourceImage(sw,sh,vw,vh);
    const struct {float rect[4];uint32_t width,height,selected,available;} constants{
        {rect.x,rect.y,rect.width,rect.height},data_?data_->maps[0].width:1,data_?data_->maps[0].height:1,
        uint32_t(selected),uint32_t(data_&&(cacheValid||selected==0||selected==3))};
    pass_.Draw(list,output,vw,vh,selected<3&&maps_[selected]?maps_[selected]->View():nullFloat_.gpu,
        maps_[3]?maps_[3]->View():nullUint_.gpu,&constants);
}
LightingCapture::LightingCapture(ID3D12Device* device,ID3D12GraphicsCommandList* list,LightingPreview& maps):cpu_(maps.Data()){
    for(size_t i=0;i<4;++i)if(maps.Map(i))reads_[i]=std::make_unique<TextureReadback>(device,list,maps.Map(i)->Image());
}
void LightingCapture::Save(const std::filesystem::path& capture)const{
    const auto directory=capture.parent_path()/(capture.stem().wstring()+L"-lighting");std::filesystem::create_directories(directory);
    package::Json report=package::Json::object();
    for(size_t i=0;i<4;++i)if(reads_[i]){const auto image=reads_[i]->Read();
        if(image.bytes!=cpu_->maps[i].bytes)throw std::runtime_error("Lighting GPU readback is not byte-exact");
        std::ofstream file(directory/(std::string(LightingKeys[i])+".bin"),std::ios::binary);file.exceptions(std::ios::badbit|std::ios::failbit);
        file.write(reinterpret_cast<const char*>(image.bytes.data()),image.bytes.size());
        report[LightingKeys[i]]={{"byteExact",true},{"nonfinite",0},{"bytes",image.bytes.size()},{"size",{image.width,image.height}},
            {"stateBefore",uint32_t(reads_[i]->Before())},{"stateCapture",uint32_t(D3D12_RESOURCE_STATE_COPY_SOURCE)},{"stateAfter",uint32_t(reads_[i]->Before())}};
    }
    std::ofstream file(directory/"readback.json",std::ios::binary);file.exceptions(std::ios::badbit|std::ios::failbit);file<<report.dump(2)<<'\n';
    Log("Lighting GPU readback: maps="+std::to_string(report.size())+" byteExact=1 nonfinite=0 states restored");
}
}
