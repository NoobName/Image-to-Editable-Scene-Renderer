#include "Scene/Mesh.h"
#include "Core/Error.h"
#include <wincodec.h>
#include <wrl/client.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <cstring>
#include <cmath>
#include <iostream>
using namespace isr;
using Microsoft::WRL::ComPtr;
namespace {
std::vector<uint8_t> Read(const std::filesystem::path& p) {std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
void Write(const std::filesystem::path& p,const void* bytes,size_t size){std::ofstream f(p,std::ios::binary);f.exceptions(std::ios::failbit|std::ios::badbit);f.write(static_cast<const char*>(bytes),static_cast<std::streamsize>(size));}
void Png(const std::filesystem::path& path,int kind){
    constexpr UINT size=64;std::vector<uint8_t> pixels(size*size*4);
    for(UINT y=0;y<size;++y)for(UINT x=0;x<size;++x){auto* p=&pixels[(y*size+x)*4];p[3]=255;
        if(kind==0){bool check=((x/8+y/8)%2)==0;p[0]=check?210:35;p[1]=check?95:165;p[2]=check?32:210;}
        if(kind==1){p[0]=static_cast<uint8_t>(64+191*float(y)/63);p[1]=static_cast<uint8_t>(32+223*float(x)/63);p[2]=y<32?0:255;}
        if(kind==2){float nx=0.35f*std::sin(x*DirectX::XM_2PI/16),ny=0.35f*std::sin(y*DirectX::XM_2PI/16);p[0]=uint8_t((nx+1)*127.5f);p[1]=uint8_t((ny+1)*127.5f);p[2]=uint8_t((std::sqrt(1-nx*nx-ny*ny)+1)*127.5f);}
        if(kind==3){bool stripe=(x%32)<3;p[0]=stripe?255:0;p[1]=stripe?140:0;p[2]=stripe?25:0;}
    }
    ComPtr<IWICImagingFactory> factory;Check(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)));
    ComPtr<IWICStream> stream;Check(factory->CreateStream(&stream));Check(stream->InitializeFromFilename(path.c_str(),GENERIC_WRITE));
    ComPtr<IWICBitmapEncoder> encoder;Check(factory->CreateEncoder(GUID_ContainerFormatPng,nullptr,&encoder));Check(encoder->Initialize(stream.Get(),WICBitmapEncoderNoCache));
    ComPtr<IWICBitmapFrameEncode> frame;Check(encoder->CreateNewFrame(&frame,nullptr));Check(frame->Initialize(nullptr));Check(frame->SetSize(size,size));
    for(size_t i=0;i<pixels.size();i+=4)std::swap(pixels[i],pixels[i+2]);
    auto format=GUID_WICPixelFormat32bppBGRA;Check(frame->SetPixelFormat(&format));if(format!=GUID_WICPixelFormat32bppBGRA)throw std::runtime_error("PNG BGRA format unavailable");
    Check(frame->WritePixels(size,size*4,static_cast<UINT>(pixels.size()),pixels.data()));Check(frame->Commit());Check(encoder->Commit());
}
}
int wmain(int argc,wchar_t** argv){
    try{
        if(argc!=2)throw std::runtime_error("Usage: GenerateAssets <output directory>");
        const HRESULT com=CoInitializeEx(nullptr,COINIT_MULTITHREADED);Check(com);
        const std::filesystem::path directory=argv[1];std::filesystem::create_directories(directory);
        std::vector<uint8_t> binary;std::ostringstream views,accessors,meshes;
        unsigned viewIndex=0,accessorIndex=0;
        auto append=[&](const void* data,size_t bytes){while(binary.size()%4)binary.push_back(0);size_t offset=binary.size();auto* first=static_cast<const uint8_t*>(data);binary.insert(binary.end(),first,first+bytes);return offset;};
        const Mesh geometry[]={Mesh::Cube(),Mesh::Sphere(32,16),Mesh::Plane()};
        for(unsigned m=0;m<3;++m){const auto& mesh=geometry[m];size_t vo=append(mesh.vertices.data(),mesh.vertices.size()*sizeof(Vertex));
            std::vector<uint16_t> indices(mesh.indices.begin(),mesh.indices.end());size_t io=append(indices.data(),indices.size()*2);
            if(m){views<<',';accessors<<',';meshes<<',';}
            views<<"{\"buffer\":0,\"byteOffset\":"<<vo<<",\"byteLength\":"<<mesh.vertices.size()*sizeof(Vertex)<<",\"byteStride\":"<<sizeof(Vertex)<<"},"
                 <<"{\"buffer\":0,\"byteOffset\":"<<io<<",\"byteLength\":"<<indices.size()*2<<"}";
            unsigned a=accessorIndex;
            auto lo=mesh.vertices.front().position,hi=lo;
            for(const auto& v:mesh.vertices){lo.x=std::min(lo.x,v.position.x);lo.y=std::min(lo.y,v.position.y);lo.z=std::min(lo.z,v.position.z);hi.x=std::max(hi.x,v.position.x);hi.y=std::max(hi.y,v.position.y);hi.z=std::max(hi.z,v.position.z);}
            accessors<<"{\"bufferView\":"<<viewIndex<<",\"componentType\":5126,\"count\":"<<mesh.vertices.size()<<",\"type\":\"VEC3\",\"min\":["<<lo.x<<','<<lo.y<<','<<lo.z<<"],\"max\":["<<hi.x<<','<<hi.y<<','<<hi.z<<"]},"
                <<"{\"bufferView\":"<<viewIndex<<",\"byteOffset\":12,\"componentType\":5126,\"count\":"<<mesh.vertices.size()<<",\"type\":\"VEC3\"},"
                <<"{\"bufferView\":"<<viewIndex<<",\"byteOffset\":40,\"componentType\":5126,\"count\":"<<mesh.vertices.size()<<",\"type\":\"VEC2\"},"
                <<"{\"bufferView\":"<<viewIndex+1<<",\"componentType\":5123,\"count\":"<<indices.size()<<",\"type\":\"SCALAR\"}";
            meshes<<"{\"primitives\":[{\"attributes\":{\"POSITION\":"<<a<<",\"NORMAL\":"<<a+1<<",\"TEXCOORD_0\":"<<a+2<<"},\"indices\":"<<a+3<<",\"material\":"<<m<<"}]}";
            accessorIndex+=4;viewIndex+=2;
        }
        const char* names[]={"base color.png","orm.png","normal.png","emissive.png"};for(int i=0;i<4;++i)Png(directory/names[i],i);
        std::ostringstream common;
        common<<"\"asset\":{\"version\":\"2.0\",\"generator\":\"ImageSceneRenderer deterministic fixture\"},\"scene\":0,\"scenes\":[{\"nodes\":[0]}],"
          "\"nodes\":[{\"name\":\"Material Lab\",\"translation\":[0,0,0],\"rotation\":[0,0.0998334,0,0.9950042],\"children\":[1,2,3,4]},"
          "{\"name\":\"Textured Cube\",\"mesh\":0,\"translation\":[-1.8,0.7,0],\"scale\":[1.4,1.4,1.4]},"
          "{\"name\":\"Textured Sphere\",\"mesh\":1,\"translation\":[0.2,1,0]},"
          "{\"name\":\"Mirrored Sphere\",\"mesh\":1,\"matrix\":[-0.75,0,0,0,0,0.75,0,0,0,0,0.75,0,2.3,0.75,0,1]},"
          "{\"name\":\"Ground\",\"mesh\":2,\"scale\":[8,1,6]}],"
          "\"samplers\":[{\"magFilter\":9729,\"minFilter\":9987,\"wrapS\":10497,\"wrapT\":10497}],"
          "\"textures\":[{\"source\":0,\"sampler\":0},{\"source\":1,\"sampler\":0},{\"source\":2,\"sampler\":0},{\"source\":3,\"sampler\":0}],"
          "\"materials\":[{\"name\":\"Painted metal\",\"pbrMetallicRoughness\":{\"baseColorTexture\":{\"index\":0},\"metallicRoughnessTexture\":{\"index\":1}},"
          "\"normalTexture\":{\"index\":2},\"occlusionTexture\":{\"index\":1,\"strength\":0.8},\"emissiveTexture\":{\"index\":3},\"emissiveFactor\":[1,0.7,0.2]},"
          "{\"name\":\"Shared textured metal\",\"pbrMetallicRoughness\":{\"baseColorFactor\":[0.8,0.9,1,1],\"baseColorTexture\":{\"index\":0},\"metallicRoughnessTexture\":{\"index\":1}},"
          "\"normalTexture\":{\"index\":2,\"scale\":0.7},\"occlusionTexture\":{\"index\":1}},"
          "{\"name\":\"Neutral ground\",\"pbrMetallicRoughness\":{\"baseColorFactor\":[0.22,0.24,0.28,1],\"metallicFactor\":0,\"roughnessFactor\":0.8}}],"
          "\"meshes\":["<<meshes.str()<<"],\"accessors\":["<<accessors.str()<<"],";
        const std::string imageUris="\"images\":[{\"uri\":\"base%20color.png\"},{\"uri\":\"orm.png\"},{\"uri\":\"normal.png\"},{\"uri\":\"emissive.png\"}],";
        const std::string json="{"+common.str()+imageUris+"\"bufferViews\":["+views.str()+"],\"buffers\":[{\"uri\":\"MaterialLab.bin\",\"byteLength\":"+std::to_string(binary.size())+"}]}";
        Write(directory/"MaterialLab.bin",binary.data(),binary.size());Write(directory/"MaterialLab.gltf",json.data(),json.size());
        std::ostringstream embedded;
        for(int i=0;i<4;++i){auto png=Read(directory/names[i]);size_t offset=append(png.data(),png.size());views<<",{\"buffer\":0,\"byteOffset\":"<<offset<<",\"byteLength\":"<<png.size()<<"}";if(i)embedded<<',';embedded<<"{\"bufferView\":"<<viewIndex++<<",\"mimeType\":\"image/png\"}";}
        std::string glbJson="{"+common.str()+"\"images\":["+embedded.str()+"],\"bufferViews\":["+views.str()+"],\"buffers\":[{\"byteLength\":"+std::to_string(binary.size())+"}]}";
        while(glbJson.size()%4)glbJson+=' ';while(binary.size()%4)binary.push_back(0);
        std::ofstream glb(directory/"MaterialLab.glb",std::ios::binary);auto word=[&](uint32_t value){glb.write(reinterpret_cast<const char*>(&value),4);};
        word(0x46546C67);word(2);word(static_cast<uint32_t>(12+8+glbJson.size()+8+binary.size()));word(static_cast<uint32_t>(glbJson.size()));word(0x4E4F534A);glb<<glbJson;
        word(static_cast<uint32_t>(binary.size()));word(0x004E4942);glb.write(reinterpret_cast<const char*>(binary.data()),binary.size());
        CoUninitialize();std::cout<<"Created MaterialLab.gltf and MaterialLab.glb\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
