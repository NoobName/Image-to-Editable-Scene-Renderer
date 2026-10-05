#include "Assets/AssetManager.h"
#include "Assets/AssetIO.h"
#include <stdexcept>
namespace isr {
Scene AssetManager::LoadModel(const std::filesystem::path& path) {
    std::error_code error;
    const auto canonical=std::filesystem::canonical(path,error);
    if(error){
        std::string message="Cannot open model: "+PathUtf8(path);
        const auto text=path.native();
        if(!text.empty()&&(text.front()==L'\''||text.back()==L'\''))
            message+="\nCMD treats single quotes as filename characters. Use double quotes: --model \"assets/models/MaterialLab/MaterialLab.gltf\"";
        message+="\nRelative paths are resolved from: "+PathUtf8(std::filesystem::current_path());
        throw std::runtime_error(message);
    }
    const auto found=models_.find(canonical);
    if (found!=models_.end()) return found->second;
    auto scene=ModelLoader{}.Load(canonical);
    models_.emplace(canonical,scene); return scene; // Editing materials never mutates the cached model.
}
}
