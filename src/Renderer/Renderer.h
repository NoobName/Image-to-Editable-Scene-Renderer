#pragma once
#include "Renderer/DeviceContext.h"
#include "Renderer/DescriptorAllocator.h"
#include "Renderer/FrameContext.h"
#include "Renderer/TrianglePass.h"
#include "Renderer/ScenePass.h"
#include "Renderer/Texture.h"
#include "Renderer/PostProcessingPipeline.h"
#include "UI/LookDevelopmentUI.h"
#include "Renderer/EnvironmentManager.h"
#include "Renderer/SkyPass.h"
#include <filesystem>
#include <array>
namespace isr {
enum class Demo { Clear, Triangle, Scene };
class Renderer {
public:
    Renderer(HWND window, uint32_t width, uint32_t height, bool warp, Demo demo, const Scene& scene, bool ui=false,const std::filesystem::path& environment={});
    ~Renderer();
    void Resize(uint32_t width, uint32_t height);
    void Render(const Scene& scene, const RenderSettings&, bool reverseOrder = false, const std::filesystem::path& capture = {});
    void UpdateUI(Scene&,RenderSettings&,InputState&);
    bool HandleMessage(HWND,UINT,WPARAM,LPARAM);
    void Finish();
    float SceneAspect() const { return sceneHeight_?float(sceneWidth_)/float(sceneHeight_):float(width_)/float(height_); }
private:
    static constexpr UINT FrameCount = 3;
    void CreateTargets();
    void CreateSceneTargets(uint32_t width,uint32_t height);
    DeviceContext context_;
    ComPtr<IDXGISwapChain3> swapChain_;
    DescriptorAllocator rtvHeap_, dsvHeap_, resourceHeap_, samplerHeap_;
    std::array<ComPtr<ID3D12Resource>, FrameCount> targets_;
    std::array<std::unique_ptr<FrameContext>, FrameCount> frames_;
    ComPtr<ID3D12GraphicsCommandList> commandList_;
    std::unique_ptr<TrianglePass> triangle_;
    std::unique_ptr<ScenePass> scenePass_;
    std::unique_ptr<ShadowPass> shadowPass_;
    std::unique_ptr<EnvironmentManager> environment_;
    std::unique_ptr<SkyPass> skyPass_;
    DescriptorAllocation lightingViews_;
    std::unique_ptr<Texture> depth_, hdr_, viewportColor_;
    DescriptorAllocation hdrSrv_;
    std::unique_ptr<PostProcessingPipeline> postProcessing_;
    std::unique_ptr<LookDevelopmentUI> inspector_;
    Demo demo_;
    uint32_t width_{}, height_{};
    uint32_t sceneWidth_{},sceneHeight_{};
};
}
