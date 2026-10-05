#pragma once
#include "Renderer/RenderSettings.h"
#include <string>
namespace isr {
bool ParseLightingOption(const std::wstring&,int& index,int argc,wchar_t** argv,RenderSettings&,bool& switchSmoke,bool& lightingTest);
}
