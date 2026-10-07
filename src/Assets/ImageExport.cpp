#include "Assets/ImageExport.h"
#include "Core/AtomicFile.h"
#include "Core/Error.h"
#include <wincodec.h>
#include <wrl/client.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <array>
namespace isr {
void ValidateNativeExportSize(uint32_t width,uint32_t height){
    if(!width||!height||width>8192||height>8192||uint64_t(width)*height>8*1024*1024)
        throw std::runtime_error("Native export limit: 8192 per edge, 8 MiPixels; no implicit resizing. Additional readback/CPU budget <= 768 MiB.");
}
void WriteRgbPng(const std::filesystem::path& path,const ImageData& image){
    using Microsoft::WRL::ComPtr;
    struct Apartment {HRESULT result=CoInitializeEx(nullptr,COINIT_MULTITHREADED);Apartment(){if(result!=RPC_E_CHANGED_MODE)Check(result);}~Apartment(){if(SUCCEEDED(result))CoUninitialize();}} apartment;
    if(image.rgba.size()!=size_t(image.width)*image.height*4)throw std::runtime_error("Invalid export pixels");
    ComPtr<IWICImagingFactory> factory;Check(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)));
    ComPtr<IWICStream> stream;Check(factory->CreateStream(&stream));Check(stream->InitializeFromFilename(path.c_str(),GENERIC_WRITE));
    ComPtr<IWICBitmapEncoder> encoder;Check(factory->CreateEncoder(GUID_ContainerFormatPng,nullptr,&encoder));Check(encoder->Initialize(stream.Get(),WICBitmapEncoderNoCache));
    ComPtr<IWICBitmapFrameEncode> frame;Check(encoder->CreateNewFrame(&frame,nullptr));Check(frame->Initialize(nullptr));Check(frame->SetSize(image.width,image.height));
    auto format=GUID_WICPixelFormat24bppBGR;Check(frame->SetPixelFormat(&format));if(format!=GUID_WICPixelFormat24bppBGR)throw std::runtime_error("PNG BGR24 encoder input unavailable");
    ComPtr<IWICMetadataQueryWriter> metadata;Check(frame->GetMetadataQueryWriter(&metadata));PROPVARIANT intent{};intent.vt=VT_UI1;intent.bVal=0;Check(metadata->SetMetadataByName(L"/sRGB/RenderingIntent",&intent));
    std::vector<uint8_t> row(size_t(image.width)*3);
    for(uint32_t y=0;y<image.height;++y){for(uint32_t x=0;x<image.width;++x){const auto* pixel=image.rgba.data()+(size_t(y)*image.width+x)*4;row[x*3]=pixel[2];row[x*3+1]=pixel[1];row[x*3+2]=pixel[0];}
        Check(frame->WritePixels(1,UINT(row.size()),UINT(row.size()),row.data()));}
    Check(frame->Commit());Check(encoder->Commit());
}
void WriteNumericDds(const std::filesystem::path& path,const NumericImage& image,bool replace){
    if(!image.width||!image.height||image.bytes.size()!=size_t(image.width)*image.height*image.Channels()*4)throw std::runtime_error("Invalid DDS export size");
    if(image.format!=NumericFormat::Vector&&image.format!=NumericFormat::Float&&image.format!=NumericFormat::Label)throw std::runtime_error("Unsupported DDS export format");
    std::array<uint32_t,37> header{};header[0]=0x20534444;header[1]=124;header[2]=0x100f;header[3]=image.height;header[4]=image.width;header[5]=image.width*image.Channels()*4;header[7]=1;
    header[19]=32;header[20]=4;header[21]=0x30315844;header[27]=0x1000;header[32]=uint32_t(image.format);header[33]=3;header[35]=1;
    std::vector<uint8_t> bytes(sizeof(header)+image.bytes.size());std::memcpy(bytes.data(),header.data(),sizeof(header));std::memcpy(bytes.data()+sizeof(header),image.bytes.data(),image.bytes.size());AtomicWrite(path,bytes,replace);
}
ImageData DisplayRgb(const NumericImage& image,float exposure,bool encodeSrgb){
    ImageData result;result.width=image.width;result.height=image.height;result.rgba.resize(size_t(image.width)*image.height*4);
    const float gain=std::exp2(exposure);
    for(size_t i=0;i<size_t(image.width)*image.height;++i){for(unsigned c=0;c<3;++c){float v=image.FloatAt(i,image.Channels()==1?0:c);
        if(!std::isfinite(v))throw std::runtime_error("NaN/Inf in export");v=std::max(0.f,v*gain);
        // Identical IEC sRGB transfer to ColorManagement.hlsli. No ACES, gamma slider or UI overlay.
        if(encodeSrgb)v=v<=.0031308f?12.92f*v:1.055f*std::pow(v,1.f/2.4f)-.055f;
        result.rgba[i*4+c]=uint8_t(std::lround(std::clamp(v,0.f,1.f)*255));}result.rgba[i*4+3]=255;}
    return result;
}
}
