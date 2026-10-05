#pragma once
#include "Renderer/DescriptorAllocator.h"
#include "Renderer/RenderSettings.h"
#include "Scene/Scene.h"
#include "App/InputState.h"
#include <vector>
#include "UI/EnvironmentPanel.h"
#include "UI/ScenePanels.h"
#include "UI/ImGuiInput.h"
#include "UI/ReconstructionPanel.h"
#include "UI/ViewportTools.h"
#include "UI/SourceImagePanel.h"
namespace isr {
class LookDevelopmentUI {
public:
    LookDevelopmentUI(HWND,DeviceContext&,unsigned frames);
    ~LookDevelopmentUI();
    LookDevelopmentUI(const LookDevelopmentUI&)=delete;
    LookDevelopmentUI& operator=(const LookDevelopmentUI&)=delete;
    void Update(Scene&,RenderSettings&,InputState&,EnvironmentManager&,RelightingSession&);
    void Draw(ID3D12GraphicsCommandList*);
    bool HandleMessage(HWND,UINT,WPARAM,LPARAM);
    void SetViewportTexture(ID3D12Resource*); // Caller must retire GPU uses before replacing the view.
    void BindReconstruction(HWND window,ReconstructionManager* manager){reconstruction_.Bind(window,manager);}
    void ResetSelection(){selection_={};edit_.Reset();viewportTools_.Reset();}
    void SelectEntity(size_t index){selection_={SelectionKind::Entity,index};}
    uint32_t ViewportWidth() const { return viewportWidth_; }
    uint32_t ViewportHeight() const { return viewportHeight_; }
private:
    void Cleanup();
    void DrawPanels(Scene&,RenderSettings&,EnvironmentManager&,RelightingSession&);
    DeviceContext& context_;
    DescriptorAllocator heap_;
    std::vector<UINT> free_;
    bool win32_=false,dx12_=false,created_=false;
    SceneSelection selection_;
    ViewportInput input_;
    uint32_t viewportWidth_=1,viewportHeight_=1;
    static constexpr UINT ViewportSlot=63;
    EnvironmentPanel environmentPanel_;
    ReconstructionPanel reconstruction_;
    SceneEditState edit_;
    ViewportTools viewportTools_;
    bool toolMouse_=false;
    bool modeChanged_=false;
};
}
