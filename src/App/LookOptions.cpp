#include "App/LookOptions.h"
namespace isr {
bool ParseLookOption(const std::wstring& argument,int& index,int argc,wchar_t** argv,LookParameters& look){
    if(argument==L"--bloom"){look.bloomEnabled=true;return true;}
    if(argument==L"--no-bloom"){look.bloomEnabled=false;return true;}
    if(argument==L"--tone-mapping"){
        if(index+1>=argc)throw std::invalid_argument("Missing tone mapping value");
        const std::wstring value=argv[++index];
        if(value==L"none")look.toneMapping=ToneMapping::None;
        else if(value==L"reinhard")look.toneMapping=ToneMapping::Reinhard;
        else if(value==L"aces")look.toneMapping=ToneMapping::ACES;
        else throw std::invalid_argument("Tone mapping must be none, reinhard or aces");
        return true;
    }
    for(const auto& p:LookParameterSchema)if(p.active&&argument==L"--"+std::wstring(p.key.begin(),p.key.end())){
        if(index+1>=argc)throw std::invalid_argument("Missing value for "+std::string(p.key));
        size_t consumed=0;const std::wstring text=argv[++index];const float value=std::stof(text,&consumed);
        if(consumed!=text.size())throw std::invalid_argument("Invalid numeric suffix for "+std::string(p.key));
        SetLookParameter(look,p.key,value);return true;
    }return false;
}
}
