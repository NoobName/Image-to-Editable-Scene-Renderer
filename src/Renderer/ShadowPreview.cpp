#include "Renderer/ShadowPreview.h"
#include "Renderer/ShaderCompiler.h"
#include "Core/Log.h"
#include <fstream>
namespace isr {
ShadowPreview::ShadowPreview(ID3D12Device* device,ID3D12GraphicsCommandList* list,Texture& source,std::shared_ptr<const ShadowData> data)
    :data_(std::move(data)),heap_(device,D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,16,true),
     pass_(device,ExecutableDirectory()/"shaders/ShadowPreview.hlsl",L"PSMain",DXGI_FORMAT_R8G8B8A8_UNORM,3,12){
    if(data_)for(size_t i=0;i<8;++i)maps_[i]=std::make_unique<NumericTexture>(device,list,heap_,data_->maps[i]);
    nullFloat_=heap_.Allocate();nullUint_=heap_.Allocate();source_=heap_.Allocate();
    D3D12_SHADER_RESOURCE_VIEW_DESC d{};d.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;
    d.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;d.Texture2D.MipLevels=1;
    d.Format=DXGI_FORMAT_R32_FLOAT;device->CreateShaderResourceView(nullptr,&d,nullFloat_.cpu);
    d.Format=DXGI_FORMAT_R32_UINT;device->CreateShaderResourceView(nullptr,&d,nullUint_.cpu);
    // Overlay works on encoded source RGB for display only. It never feeds relighting.
    d.Format=DXGI_FORMAT_R8G8B8A8_UNORM;device->CreateShaderResourceView(source.Resource(),&d,source_.cpu);
}
void ShadowPreview::FinishUpload(){for(auto& map:maps_)if(map)map->FinishUpload();}
void ShadowPreview::Draw(ID3D12GraphicsCommandList* list,D3D12_CPU_DESCRIPTOR_HANDLE output,uint32_t vw,uint32_t vh,uint32_t sw,uint32_t sh,size_t selected,bool cacheValid)const{
    if(selected>8)throw std::out_of_range("Unknown shadow preview");const auto rect=FitSourceImage(sw,sh,vw,vh);
    const struct {float rect[4];uint32_t sw,sh,aw,ah,selected,available,cacheValid,pad;} constants{{rect.x,rect.y,rect.width,rect.height},sw,sh,
        data_?data_->maps[0].width:1,data_?data_->maps[0].height:1,uint32_t(selected),uint32_t(data_!=nullptr),uint32_t(cacheValid),0};
    const auto i=selected==8?7:selected;
    const std::array<D3D12_GPU_DESCRIPTOR_HANDLE,3> views{data_&&i!=4?maps_[i]->View():nullFloat_.gpu,data_?maps_[4]->View():nullUint_.gpu,source_.gpu};
    ID3D12DescriptorHeap* heaps[]{heap_.Heap()};list->SetDescriptorHeaps(1,heaps);pass_.Draw(list,output,vw,vh,views,&constants);
}
ShadowCapture::ShadowCapture(ID3D12Device* device,ID3D12GraphicsCommandList* list,ShadowPreview& maps):cpu_(maps.Data()){
    for(size_t i=0;i<8;++i)if(maps.Map(i))reads_[i]=std::make_unique<TextureReadback>(device,list,maps.Map(i)->Image());
}
void ShadowCapture::Save(const std::filesystem::path& capture)const{
    const auto directory=capture.parent_path()/(capture.stem().wstring()+L"-shadow");std::filesystem::create_directories(directory);package::Json report=package::Json::object();
    for(size_t i=0;i<8;++i)if(reads_[i]){const auto image=reads_[i]->Read();if(image.bytes!=cpu_->maps[i].bytes)throw std::runtime_error("Shadow GPU readback is not byte-exact");
        std::ofstream file(directory/(std::string(ShadowKeys[i])+".bin"),std::ios::binary);file.exceptions(std::ios::badbit|std::ios::failbit);file.write(reinterpret_cast<const char*>(image.bytes.data()),image.bytes.size());
        report[ShadowKeys[i]]={{"byteExact",true},{"nonfinite",0},{"size",{image.width,image.height}},{"stateBefore",uint32_t(reads_[i]->Before())},{"stateCapture",uint32_t(D3D12_RESOURCE_STATE_COPY_SOURCE)},{"stateAfter",uint32_t(reads_[i]->Before())}};}
    std::ofstream out(directory/"readback.json",std::ios::binary);out.exceptions(std::ios::badbit|std::ios::failbit);out<<report.dump(2)<<'\n';Log("Shadow GPU readback: maps="+std::to_string(report.size())+" byteExact=1 nonfinite=0 states restored");
}
}
