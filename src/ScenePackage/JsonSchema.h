#pragma once
#include <nlohmann/json.hpp>
#include <filesystem>
namespace isr::package {
using Json = nlohmann::json;
// Implements only the keyword profile used by our shared schema; never fetches remote refs.
const Json& Schema();
Json ReadJson(const std::filesystem::path& path);
void Validate(const Json& value, const Json& rule, const std::string& location = "$");
void ApplyDefaults(Json& value, const Json& rule);
}
