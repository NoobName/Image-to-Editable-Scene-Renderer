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
#include <future>
#include "Renderer/PreparedScene.h"
#include "Renderer/ImageGpuTimer.h"
#include "Renderer/ReferencePreview.h"
#include "Renderer/FrameProfiler.h"
namespace isr {
enum class Demo { Clear, Triangle, Scene };
class Renderer {
public:
    Renderer(HWND window, uint32_t width, uint32_t height, bool warp, Demo demo, const Scene& scene, bool ui=false,const std::filesystem::path& environment={},std::shared_ptr<const SourceObservation> source={},std::shared_ptr<const ProtectionMask> protection={});
    ~Renderer();
    void Resize(uint32_t width, uint32_t height);
    void Render(const Scene& scene, const RenderSettings&, bool reverseOrder = false, const std::filesystem::path& capture = {});
    void UpdateUI(Scene&,RenderSettings&,InputState&);
    bool HandleMessage(HWND,UINT,WPARAM,LPARAM);
    void Finish();
    void EnableProfiling(){profiler_=std::make_unique<FrameProfiler>(context_);imageTimer_->SetWarmup(60);}
    void ProfileCpuFrame(double ms,bool capture){if(profiler_)profiler_->CpuFrame(ms,capture);}
    void SaveProfile(const std::filesystem::path&);
    void ExportImage(const std::filesystem::path&);
    void PublishReference(std::shared_ptr<const ReferenceAnalysis>);
    double VerifyOptimizationCandidate(const ReferenceAnalysis&);
    void BindRecipe(HWND window,RecipeActions* actions){if(inspector_)inspector_->BindRecipe(window,actions);}
    void BindReference(HWND window,ReferenceActions* actions){if(inspector_)inspector_->BindReference(window,actions);}
    const std::shared_ptr<const ProtectionMask>& ImportedProtection()const{return protection_;}
    void BindReconstruction(HWND window,ReconstructionManager* manager){if(inspector_)inspector_->BindReconstruction(window,manager);}
    void SelectEntity(size_t index){if(inspector_)inspector_->SelectEntity(index);}
    void PrepareScene(std::shared_ptr<const ScenePackage>,std::shared_ptr<const ProtectionMask> = {},bool replaceProtection=false);
    bool ScenePrepared()const;
    void CommitPreparedScene(bool discard=false,RelightingSession* replacement=nullptr);
    RelightingSession& Session(){return session_;}
    const RelightingSession& Session()const{return session_;}
    uint32_t ViewWidth()const{return sceneWidth_;}
    uint32_t ViewHeight()const{return sceneHeight_;}
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
    std::unique_ptr<SourceImagePass> sourceImage_;
    std::unique_ptr<AnalysisTextures> analysis_;
    std::unique_ptr<LightingPreview> lightingPreview_;
    std::unique_ptr<IntrinsicPreview> intrinsicPreview_;
    std::unique_ptr<ShadowPreview> shadowPreview_;
    std::unique_ptr<ImageRelightingRenderer> imageRelighting_;
    std::unique_ptr<ImageRelightingComposite> imageComposite_;
    std::unique_ptr<ImageGpuTimer> imageTimer_;
    std::unique_ptr<FrameProfiler> profiler_;
    std::shared_ptr<const ProtectionMask> protection_;
    std::string protectionSourceId_;
    RelightingSession session_;
    std::unique_ptr<ReferencePreview> referencePreview_;
    Demo demo_;
    uint32_t width_{}, height_{};
    uint32_t sceneWidth_{},sceneHeight_{};
    std::future<std::unique_ptr<PreparedScene>> preparing_;
    std::unique_ptr<PreparedScene> activeScene_;
    struct RetiredScene {uint64_t fence;std::unique_ptr<PreparedScene> prepared;
        std::unique_ptr<ScenePass> scene;std::unique_ptr<ShadowPass> shadow;std::unique_ptr<SourceImagePass> sourceImage;std::unique_ptr<AnalysisTextures> analysis;std::unique_ptr<LightingPreview> lightingPreview;std::unique_ptr<ImageRelightingRenderer> imageRelighting;std::unique_ptr<ImageRelightingComposite> imageComposite;std::unique_ptr<IntrinsicPreview> intrinsicPreview;std::unique_ptr<ShadowPreview> shadowPreview;};
    std::vector<RetiredScene> retired_;
};
}
