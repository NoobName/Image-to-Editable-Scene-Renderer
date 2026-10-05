#pragma once
#include "Scene/Scene.h"
#include <filesystem>
namespace isr {
class ModelLoader {
public:
    Scene Load(const std::filesystem::path& path, const std::filesystem::path& allowedRoot = {}) const;
};
}
