#pragma once
#include "Scene/LookParameters.h"
#include <string>
namespace isr {
bool ParseLookOption(const std::wstring&,int& index,int argc,wchar_t** argv,LookParameters&);
}
