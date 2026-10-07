#pragma once
#include "Renderer/Renderer.h"
namespace isr {
struct ImageModeOptions {
    WorkMode initial=WorkMode::Scene3D;
    ImageDebugView view=ImageDebugView::Relighted;
    uint32_t width=1280,height=720;
    bool fixedSize=false;
    bool noSpecular=false;
    std::optional<float> fogDensity;
    bool fogRelative=false;
    void ApplyFog(Renderer& renderer)const{auto& fog=renderer.Session().fog;if(fogDensity){fog.enabled=true;fog.density=*fogDensity;}if(fogRelative)fog.allowRelativeScale=true;}
    std::string smoke;
    std::filesystem::path protectionMask;
    bool Parse(const std::wstring&,int&,int,wchar_t**);
    void Start(Renderer&);
    void Tick(unsigned,Scene&,Renderer&,InputState&);
    void Report(const std::filesystem::path&,const Scene&,const Renderer&,const RenderSettings&)const;
    bool Busy()const{return smoke=="transaction"&&transactionStage_!=3;}
private:
    bool waitForSource_=false;
    int transactionStage_=0;
    std::shared_ptr<const SourceObservation> previousSource_;
    uint64_t previousRevision_=0;
    void TransactionSmoke(Scene&,Renderer&);
};
}
