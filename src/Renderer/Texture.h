#pragma once
#include "Renderer/DeviceContext.h"
namespace isr {
class Texture {
public:
    Texture(ID3D12Device*, UINT width, UINT height, DXGI_FORMAT, D3D12_RESOURCE_FLAGS,
        D3D12_RESOURCE_STATES initialState, const D3D12_CLEAR_VALUE* clear = nullptr);
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    ID3D12Resource* Resource() const { return resource_.Get(); }
    D3D12_RESOURCE_STATES State()const{return state_;}
    void Transition(ID3D12GraphicsCommandList*, D3D12_RESOURCE_STATES next);
private:
    ComPtr<ID3D12Resource> resource_;
    D3D12_RESOURCE_STATES state_;
};
}
