#include "Assets/ImageDecoder.h"
#include "Core/Error.h"
#include <wincodec.h>
#include <wrl/client.h>
#include <limits>
namespace isr {
std::shared_ptr<ImageData> DecodeImage(std::span<const uint8_t> bytes, const std::string& name,uint64_t maxPixels) {
    using Microsoft::WRL::ComPtr;
    struct Apartment {
        HRESULT result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        Apartment() { if (result != RPC_E_CHANGED_MODE) Check(result); }
        ~Apartment() { if (SUCCEEDED(result)) CoUninitialize(); }
    } apartment;
    if (bytes.empty() || bytes.size() > std::numeric_limits<DWORD>::max()) throw std::runtime_error("Invalid image length: " + name);
    ComPtr<IWICImagingFactory> factory;
    Check(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)));
    ComPtr<IWICStream> stream; Check(factory->CreateStream(&stream));
    Check(stream->InitializeFromMemory(const_cast<BYTE*>(bytes.data()),static_cast<DWORD>(bytes.size())));
    ComPtr<IWICBitmapDecoder> decoder; Check(factory->CreateDecoderFromStream(stream.Get(),nullptr,WICDecodeMetadataCacheOnLoad,&decoder));
    UINT frames=0;Check(decoder->GetFrameCount(&frames));
    if(frames!=1)throw std::runtime_error("Expected a single-frame image: "+name);
    ComPtr<IWICBitmapFrameDecode> frame; Check(decoder->GetFrame(0,&frame));
    auto image = std::make_shared<ImageData>(); image->name = name;
    Check(frame->GetSize(&image->width,&image->height));
    if (!image->width || !image->height || image->width > 16384 || image->height > 16384 ||
        uint64_t(image->width)*image->height>maxPixels ||
        uint64_t(image->width)*image->height*4 > 512ull*1024*1024) throw std::runtime_error("Image dimensions exceed limits: " + name);
    ComPtr<IWICFormatConverter> converter; Check(factory->CreateFormatConverter(&converter));
    Check(converter->Initialize(frame.Get(),GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom));
    image->rgba.resize(size_t(image->width)*image->height*4);
    // No WIC color transform: glTF assigns sRGB/linear interpretation by texture role.
    Check(converter->CopyPixels(nullptr,image->width*4,static_cast<UINT>(image->rgba.size()),image->rgba.data()));
    return image;
}
}
