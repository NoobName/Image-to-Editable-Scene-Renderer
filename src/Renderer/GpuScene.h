#pragma once
#include "Renderer/GpuBuffer.h"
#include "Renderer/TextureManager.h"
#include "Renderer/ShaderConstants.h"
namespace isr {
struct UploadedMesh {
    std::unique_ptr<GpuBuffer> vertices,indices;
    UINT indexCount{};
    void Draw(ID3D12GraphicsCommandList*) const;
};
class GpuScene {
public:
    GpuScene(ID3D12Device*,ID3D12GraphicsCommandList*,DescriptorAllocator&,DescriptorAllocator&,const Scene&);
    void FinishUpload();
    const UploadedMesh& Mesh(size_t i) const{return meshes_.at(i);}
    const MaterialBinding& Binding(size_t i) const{return textures_->Binding(i);}
    static D3D12_INPUT_LAYOUT_DESC InputLayout();
    static ObjectConstants Constants(const Entity&,const Material&,DirectX::FXMMATRIX viewProjection);
private:
    std::vector<UploadedMesh> meshes_;
    std::unique_ptr<TextureManager> textures_;
};
}
