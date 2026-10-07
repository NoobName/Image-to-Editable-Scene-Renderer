#include "Renderer/Renderer.h"
#include "Renderer/TextureReadback.h"
#include "Assets/ImageExport.h"
#include "Assets/AssetIO.h"
#include "Core/AtomicFile.h"
#include "Core/Log.h"
#include "ScenePackage/RelightingRecipe.h"
#include <atomic>
#include <cstring>
namespace isr {
void Renderer::ExportImage(const std::filesystem::path& output){
    if(!session_.CanDisplayImage()||session_.Mode()!=WorkMode::ImageRelighting)throw std::runtime_error("Export requires Image Relighting mode and a source anchor");
    const auto& anchor=*session_.Source()->anchor;const auto& size=anchor.sourceSize;
    ValidateNativeExportSize(size[0],size[1]);
    const auto destination=std::filesystem::absolute(output);
    if(std::filesystem::exists(destination))throw std::runtime_error("Export destination already exists; choose a new directory");
    // Export cannot write inside the immutable package (also catches a symlinked parent).
    const auto parent=std::filesystem::canonical(destination.parent_path());
    auto root=std::filesystem::canonical(session_.Source()->packageRoot);auto d=parent.begin();bool inside=true;
    for(auto r=root.begin();r!=root.end();++r,++d)if(d==parent.end()||_wcsicmp(d->c_str(),r->c_str())!=0){inside=false;break;}
    if(inside)throw std::runtime_error("Export outside the immutable source package");
    auto* composite=activeScene_?activeScene_->imageComposite.get():imageComposite_.get();
    auto* shading=activeScene_?activeScene_->imageRelighting.get():imageRelighting_.get();
    context_.Flush();auto& frame=*frames_[0];frame.Begin(context_);Check(commandList_->Reset(frame.commandAllocator.Get(),nullptr));
    shading->Update(commandList_.Get(),session_.lighting,session_.pointLights);composite->EditProtection(context_.Device(),commandList_.Get(),frame,*session_.Source(),session_.protection);
    composite->Update(commandList_.Get(),session_.relighting,session_.lighting.cacheValid,&frame,session_.fog);
    std::array<Texture*,7> textures{composite->Final().Image().texture,shading->Old().Image().texture,shading->New().Image().texture,composite->Ratio().Image().texture,&composite->Quality(0),
        &composite->Fog()->Distance(),composite->PreFog().Image().texture};
    std::array<std::unique_ptr<TextureReadback>,7> reads;
    for(size_t i=0;i<reads.size();++i)reads[i]=std::make_unique<TextureReadback>(context_.Device(),commandList_.Get(),*textures[i]);
    Check(commandList_->Close());ID3D12CommandList* lists[]{commandList_.Get()};context_.Queue()->ExecuteCommandLists(1,lists);frame.fenceValue=context_.Signal();context_.Wait(frame.fenceValue);context_.CheckMessages();
    static std::atomic_uint64_t counter=0;auto temporary=destination;temporary+=L".pending-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(++counter);
    if(!std::filesystem::create_directory(temporary))throw std::runtime_error("Cannot create export staging directory");
    struct Cleanup{std::filesystem::path path;~Cleanup(){std::error_code ec;std::filesystem::remove_all(path,ec);}} cleanup{temporary};
    package::Json report={{"version",1},{"encoding","display-referred sRGB PNG; linear-sRGB display-referred float DDS, NOT HDR radiance"},
        {"sourceId",anchor.metadata.at("sourceId")},{"anchorSha256",anchor.metadata.at("sourceImage").at("sha256")},{"nativeSize",size},{"state",RecipeState(session_)},
        {"displayExposure",session_.display.exposure},{"overlayExported",false},{"readbackBudgetMiB",768},{"shading",shading->Report()},{"composition",composite->Report()}};
    WriteRgbPng(temporary/"original.png",*anchor.pixels);
    constexpr const char* names[]{"result","old-shading","new-shading","ratio","quality","fog-distance","pre-fog"};
    for(size_t i=0;i<reads.size();++i){const auto image=reads[i]->Read();
        if(i==0&&(image.width!=size[0]||image.height!=size[1]))throw std::runtime_error("Native result unavailable (GPU budget); refusing analysis-resolution export");
        WriteNumericDds(temporary/(std::string(names[i])+".dds"),image);
        WriteRgbPng(temporary/(std::string(names[i])+".png"),DisplayRgb(image,i==0?session_.display.exposure:0,i<3));
        if(i==3){NumericImage confidence{image.width,image.height,NumericFormat::Float,{}};confidence.bytes.resize(size_t(image.width)*image.height*4);
            for(size_t p=0;p<size_t(image.width)*image.height;++p){const float v=image.FloatAt(p,3);std::memcpy(confidence.bytes.data()+p*4,&v,4);}
            WriteNumericDds(temporary/"confidence.dds",confidence);WriteRgbPng(temporary/"confidence.png",DisplayRgb(confidence,0,false));}
        report["buffers"][names[i]]={{"size",{image.width,image.height}},{"nonfinite",0},{"before",uint32_t(reads[i]->Before())},{"capture",2048},{"after",uint32_t(textures[i]->State())}};
    }
    const auto json=report.dump(2)+"\n";AtomicWrite(temporary/"export.json",std::span(reinterpret_cast<const uint8_t*>(json.data()),json.size()));
    // The destination did not exist. Publishing the entire directory makes partial PNG/DDS output invisible.
    CheckWin32(MoveFileExW(temporary.c_str(),destination.c_str(),MOVEFILE_WRITE_THROUGH));Log("Native image export published: "+PathUtf8(destination));
}
}
