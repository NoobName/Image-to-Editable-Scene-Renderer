#include "Reconstruction/ReconstructionManager.h"
namespace isr {
std::pair<std::vector<std::string>,std::vector<std::string>> ParseProgressTable(const package::Json& data){
    const std::vector<std::string> legacy{"geometry","segmentation","materials","export"},
        full{"geometry","segmentation","materials","lighting","export"},offline{"lighting","export"};
    std::vector<std::string> names,states;
    if(data.at("version")==1)names=legacy;
    else if(data.at("version")==2){names=data.at("stage_order").get<std::vector<std::string>>();
        if(names!=full&&names!=offline&&names!=legacy)throw std::runtime_error("Unsupported explicit progress stage table");}
    else throw std::runtime_error("Unsupported progress version");
    if(!data.at("stages").is_object()||data.at("stages").size()!=names.size())throw std::runtime_error("Progress stage count mismatch");
    for(const auto& name:names){auto state=data.at("stages").at(name).get<std::string>();
        if(state!="pending"&&state!="running"&&state!="complete"&&state!="error")throw std::runtime_error("Invalid progress stage state");
        states.push_back(std::move(state));}
    return {names,states};
}
}
