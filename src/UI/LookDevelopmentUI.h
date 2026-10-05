#pragma once
#include "Renderer/DescriptorAllocator.h"
#include "Renderer/RenderSettings.h"
#include "Scene/Scene.h"
#include "App/InputState.h"
#include <vector>
#include "UI/EnvironmentPanel.h"
#include "UI/ScenePanels.h"
#include "UI/ImGuiInput.h"
namespace isr {
class LookDevelopmentUI {
public:
    LookDevelopmentUI(HWND,DeviceContext&,unsigned frames);
    ~LookDevelopmentUI();
    LookDevelopmentUI(const LookDevelopmentUI&)=delete;
    LookDevelopmentUI& operator=(const LookDevelopmentUI&)=delete;
    void Update(Scene&,RenderSettings&,InputState&,EnvironmentManager&);
    void Draw(ID3D12GraphicsCommandList*);
    bool HandleMessage(HWND,UINT,WPARAM,LPARAM);
    void SetViewportTexture(ID3D12Resource*); // Caller must retire GPU uses before replacing the view.
    uint32_t ViewportWidth() const { return viewportWidth_; }
    uint32_t ViewportHeight() const { return viewportHeight_; }
private:
    void Cleanup();
    void DrawPanels(Scene&,RenderSettings&,EnvironmentManager&);
    DeviceContext& context_;
    DescriptorAllocator heap_;
    std::vector<UINT> free_;
    bool win32_=false,dx12_=false,created_=false;
    SceneSelection selection_;
    ViewportInput input_;
    uint32_t viewportWidth_=1,viewportHeight_=1;
    static constexpr UINT ViewportSlot=63;
    EnvironmentPanel environmentPanel_;
};
}
