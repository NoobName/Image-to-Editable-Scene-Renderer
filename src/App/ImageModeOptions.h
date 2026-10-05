#pragma once
#include "Renderer/Renderer.h"
namespace isr {
struct ImageModeOptions {
    WorkMode initial=WorkMode::Scene3D;
    ImageDebugView view=ImageDebugView::Original;
    uint32_t width=1280,height=720;
    bool fixedSize=false;
    std::string smoke;
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
