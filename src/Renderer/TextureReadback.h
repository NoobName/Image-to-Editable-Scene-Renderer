#pragma once
#include "Renderer/Texture.h"
#include "Assets/NumericDds.h"
namespace isr {
// Captures a tracked Texture, restores its actual state; caller waits for submission fence before Read.
class TextureReadback {
public:
    TextureReadback(ID3D12Device*,ID3D12GraphicsCommandList*,Texture&);
    NumericImage Read()const;
    D3D12_RESOURCE_STATES Before()const{return before_;}
private:
    ComPtr<ID3D12Resource> buffer_;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint_{};
    UINT64 bytes_{};
    NumericImage layout_;
    D3D12_RESOURCE_STATES before_{};
};
}
