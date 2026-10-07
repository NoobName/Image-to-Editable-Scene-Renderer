#include "UI/LookDevelopmentUI.h"
#include "Core/Log.h"
#include "UI/ImGuiInput.h"
#include <imgui.h>
#include <backends/imgui_impl_win32.h>
#include <backends/imgui_impl_dx12.h>
#include <algorithm>
#include <filesystem>
#include <imgui_internal.h>
namespace isr {
namespace {
void ConfigureChineseUI(){
    auto& io=ImGui::GetIO();
    // Load a system font rather than redistributing Windows font files. Keep the
    // ranges alive for the atlas, including symbols used by numerical panels.
    static ImVector<ImWchar> ranges;
    if(ranges.empty()){
        ImFontGlyphRangesBuilder builder;builder.AddRanges(io.Fonts->GetGlyphRangesChineseFull());
        builder.AddText("ε→");builder.BuildRanges(&ranges);
    }
    wchar_t windows[MAX_PATH]{};
    const auto length=GetWindowsDirectoryW(windows,MAX_PATH);
    if(!length||length>=MAX_PATH)throw std::runtime_error("无法定位 Windows 中文字体目录");
    ImFontConfig config;config.OversampleH=1;config.OversampleV=1;
    for(const auto* name:{L"msyh.ttc",L"simhei.ttf",L"simsun.ttc"}){
        const auto path=std::filesystem::path(windows)/L"Fonts"/name;
        if(!std::filesystem::is_regular_file(path))continue;
        const auto utf8=path.u8string();
        if(auto* font=io.Fonts->AddFontFromFileTTF(reinterpret_cast<const char*>(utf8.c_str()),16.f,&config,ranges.Data)){io.FontDefault=font;break;}
    }
    if(!io.FontDefault)throw std::runtime_error("未找到可用的中文字体，请安装 Windows 简体中文字体");
    static const ImGuiLocEntry entries[]{
        {ImGuiLocKey_VersionStr,"Dear ImGui 版本 " IMGUI_VERSION},{ImGuiLocKey_TableSizeOne,"调整列宽###SizeOne"},
        {ImGuiLocKey_TableSizeAllFit,"自动适应所有列###SizeAll"},{ImGuiLocKey_TableSizeAllDefault,"重置所有列宽###SizeAll"},
        {ImGuiLocKey_TableReset,"重置###Reset"},{ImGuiLocKey_TableResetOrder,"重置顺序###ResetOrder"},
        {ImGuiLocKey_TableResetVisibility,"重置可见性###ResetVisibility"},
        {ImGuiLocKey_WindowingMainMenuBar,"主菜单"},{ImGuiLocKey_WindowingPopup,"弹出窗口"},{ImGuiLocKey_WindowingUntitled,"未命名"},
        {ImGuiLocKey_OpenLink_s,"打开链接：%s"},{ImGuiLocKey_CopyLink,"复制链接###CopyLink"}
    };
    ImGui::LocalizeRegisterEntries(entries,IM_ARRAYSIZE(entries));
}
}
LookDevelopmentUI::LookDevelopmentUI(HWND window,DeviceContext& context,unsigned frames)
    :context_(context),heap_(context.Device(),D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,64,true){
    try{
        IMGUI_CHECKVERSION();if(!ImGui::CreateContext())throw std::runtime_error("ImGui context creation failed");created_=true;
        ImGui::GetIO().IniFilename=nullptr;ImGui::GetIO().ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard;
        ConfigureChineseUI();
        ImGui::StyleColorsDark();auto& style=ImGui::GetStyle();style.WindowRounding=7;style.FrameRounding=4;
        style.Colors[ImGuiCol_WindowBg]=ImVec4(0.055f,0.075f,0.10f,0.96f);
        if(!(win32_=ImGui_ImplWin32_Init(window)))throw std::runtime_error("ImGui Win32 init failed");
        heap_.Allocate(64);for(UINT i=ReferenceResidualSlot;i>0;--i)free_.push_back(i-1);
        ImGui_ImplDX12_InitInfo info{};info.Device=context.Device();info.CommandQueue=context.Queue();info.NumFramesInFlight=static_cast<int>(frames);
        info.RTVFormat=DXGI_FORMAT_R8G8B8A8_UNORM;info.DSVFormat=DXGI_FORMAT_UNKNOWN;info.UserData=this;info.SrvDescriptorHeap=heap_.Heap();
        info.SrvDescriptorAllocFn=[](ImGui_ImplDX12_InitInfo* info,D3D12_CPU_DESCRIPTOR_HANDLE* cpu,D3D12_GPU_DESCRIPTOR_HANDLE* gpu){
            auto& self=*static_cast<LookDevelopmentUI*>(info->UserData);if(self.free_.empty())throw std::runtime_error("ImGui descriptor pool exhausted");
            const auto index=self.free_.back();self.free_.pop_back();*cpu=self.heap_.Cpu(index);*gpu=self.heap_.Gpu(index);
        };
        info.SrvDescriptorFreeFn=[](ImGui_ImplDX12_InitInfo* info,D3D12_CPU_DESCRIPTOR_HANDLE cpu,D3D12_GPU_DESCRIPTOR_HANDLE){
            auto& self=*static_cast<LookDevelopmentUI*>(info->UserData);
            // Font atlas changes are rare. Retire submitted uses before recycling a descriptor.
            self.context_.Flush();for(UINT i=0;i<self.heap_.Capacity();++i)if(self.heap_.Cpu(i).ptr==cpu.ptr){self.free_.push_back(i);return;}
        };
        if(!(dx12_=ImGui_ImplDX12_Init(&info)))throw std::runtime_error("ImGui DX12 init failed");
        if(!ImGui_ImplDX12_CreateDeviceObjects())throw std::runtime_error("ImGui device object creation failed");
    }catch(...){Cleanup();throw;}
}
void LookDevelopmentUI::Cleanup(){if(dx12_)ImGui_ImplDX12_Shutdown();if(win32_)ImGui_ImplWin32_Shutdown();if(created_)ImGui::DestroyContext();dx12_=win32_=created_=false;}
LookDevelopmentUI::~LookDevelopmentUI(){try{context_.Flush();Cleanup();}catch(const std::exception& e){Log(e.what());}}
bool LookDevelopmentUI::HandleMessage(HWND window,UINT message,WPARAM wp,LPARAM lp){
    return input_.HandleMessage(window,message,wp,lp);
}
void LookDevelopmentUI::SetViewportTexture(ID3D12Resource* texture){
    D3D12_SHADER_RESOURCE_VIEW_DESC srv{};srv.Format=DXGI_FORMAT_R8G8B8A8_UNORM;srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;
    srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;srv.Texture2D.MipLevels=1;
    context_.Device()->CreateShaderResourceView(texture,&srv,heap_.Cpu(ViewportSlot));
}
void LookDevelopmentUI::SetSourceTexture(ID3D12Resource* texture){
    if(texture==sourceTexture_)return;
    // Only package commits replace this borrowed view. Drain previous UI uses before reusing its fixed slot.
    context_.Flush();D3D12_SHADER_RESOURCE_VIEW_DESC srv{};srv.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;srv.Texture2D.MipLevels=1;
    context_.Device()->CreateShaderResourceView(texture,&srv,heap_.Cpu(SourceSlot));sourceTexture_=texture;
    Log("UI source view rebound after fence: fixedSlot=62 freeFontSlots="+std::to_string(free_.size()));
}
void LookDevelopmentUI::SetReferenceTextures(ID3D12Resource* image,ID3D12Resource* residual){
    showReference_=image!=nullptr;
    context_.Flush();D3D12_SHADER_RESOURCE_VIEW_DESC srv{};srv.Format=DXGI_FORMAT_R8G8B8A8_UNORM;srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;
    srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;srv.Texture2D.MipLevels=1;
    context_.Device()->CreateShaderResourceView(image,&srv,heap_.Cpu(ReferenceSlot));context_.Device()->CreateShaderResourceView(residual,&srv,heap_.Cpu(ReferenceResidualSlot));
    Log("Reference UI views rebound after fence: fixedSlots=61,60 freeFontSlots="+std::to_string(free_.size()));
}
void LookDevelopmentUI::Update(Scene& scene,RenderSettings& settings,InputState& input,EnvironmentManager& environment,RelightingSession& session){
    if(!edit_.Captured())edit_.Capture(scene);
    ImGui_ImplDX12_NewFrame();ImGui_ImplWin32_NewFrame();ImGui::NewFrame();
    DrawPanels(scene,settings,environment,session);input_.Filter(input);
    if(modeChanged_){input_.CancelDrag();viewportTools_.Reset();}
    if(toolMouse_||modeChanged_||session.Mode()==WorkMode::ImageRelighting){input.rightMouse=false;input.ClearDeltas();input.keys.fill(false);}ImGui::Render();
}
void LookDevelopmentUI::Draw(ID3D12GraphicsCommandList* list){ID3D12DescriptorHeap* heaps[]={heap_.Heap()};list->SetDescriptorHeaps(1,heaps);ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(),list);}
}

