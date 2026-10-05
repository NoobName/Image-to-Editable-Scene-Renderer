#include "App/LightingOptions.h"
#include <cmath>
#include <stdexcept>
#include <DirectXMath.h>
namespace isr {
bool ParseLightingOption(const std::wstring& option,int& i,int argc,wchar_t** argv,RenderSettings& settings,bool& switchSmoke,bool& lightingTest){
    auto value=[&](){if(i+1>=argc)throw std::invalid_argument("Missing lighting option value");return argv[++i];};
    auto number=[&](float low,float high){std::wstring token=value();size_t parsed=0;float v=std::stof(token,&parsed);if(parsed!=token.size()||!std::isfinite(v)||v<low||v>high)throw std::invalid_argument("Lighting option outside supported range");return v;};
    if(option==L"--env")settings.environmentPath=value();
    else if(option==L"--no-ibl")settings.ibl=false;
    else if(option==L"--no-sky")settings.skybox=false;
    else if(option==L"--no-shadows")settings.shadows=false;
    else if(option==L"--env-intensity")settings.environmentIntensity=number(0,100);
    else if(option==L"--env-rotation")settings.environmentRotation=number(-3600,3600)*DirectX::XM_PI/180;
    else if(option==L"--shadow-bias")settings.shadowBias=number(0,0.05f);
    else if(option==L"--normal-bias")settings.shadowNormalBias=number(0,1);
    else if(option==L"--shadow-pcf"){const auto v=number(0,2);if(v!=std::floor(v))throw std::invalid_argument("PCF radius must be 0, 1 or 2");settings.shadowPcfRadius=int(v);}
    else if(option==L"--environment-smoke")switchSmoke=true;
    else if(option==L"--lighting-test")lightingTest=true;
    else return false;
    return true;
}
}
