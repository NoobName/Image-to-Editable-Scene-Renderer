#include "UI/LookDevelopmentUI.h"
#include "Core/Log.h"
#include "UI/ImGuiInput.h"
#include <imgui.h>
#include <backends/imgui_impl_win32.h>
#include <backends/imgui_impl_dx12.h>
#include <algorithm>
namespace isr {
LookDevelopmentUI::LookDevelopmentUI(HWND window,DeviceContext& context,unsigned frames)
    :context_(context),heap_(context.Device(),D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,64,true){
    try{
        IMGUI_CHECKVERSION();if(!ImGui::CreateContext())throw std::runtime_error("ImGui context creation failed");created_=true;
        ImGui::GetIO().IniFilename=nullptr;ImGui::GetIO().ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard;
        ImGui::StyleColorsDark();auto& style=ImGui::GetStyle();style.WindowRounding=7;style.FrameRounding=4;
        style.Colors[ImGuiCol_WindowBg]=ImVec4(0.055f,0.075f,0.10f,0.96f);
        if(!(win32_=ImGui_ImplWin32_Init(window)))throw std::runtime_error("ImGui Win32 init failed");
        heap_.Allocate(64);for(UINT i=ViewportSlot;i>0;--i)free_.push_back(i-1);
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
void LookDevelopmentUI::Update(Scene& scene,RenderSettings& settings,InputState& input,EnvironmentManager& environment,RelightingSession& session){
    if(!edit_.Captured())edit_.Capture(scene);
    ImGui_ImplDX12_NewFrame();ImGui_ImplWin32_NewFrame();ImGui::NewFrame();
    DrawPanels(scene,settings,environment,session);input_.Filter(input);
    if(modeChanged_){input_.CancelDrag();viewportTools_.Reset();}
    if(toolMouse_||modeChanged_||session.Mode()==WorkMode::ImageRelighting){input.rightMouse=false;input.ClearDeltas();input.keys.fill(false);}ImGui::Render();
}
void LookDevelopmentUI::Draw(ID3D12GraphicsCommandList* list){ID3D12DescriptorHeap* heaps[]={heap_.Heap()};list->SetDescriptorHeaps(1,heaps);ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(),list);}
}

