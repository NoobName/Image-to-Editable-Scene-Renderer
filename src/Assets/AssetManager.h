#pragma once
#include "Assets/ModelLoader.h"
#include <map>
namespace isr {
class AssetManager {
public:
    Scene LoadModel(const std::filesystem::path& path);
    size_t ModelCount() const { return models_.size(); }
private:
    std::map<std::filesystem::path,Scene> models_;
};
}
