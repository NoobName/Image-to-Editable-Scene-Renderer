#include "ScenePackage/JsonSchema.h"
#include "EmbeddedSchema.h"
#include "Assets/AssetIO.h"
#include <fstream>
#include <set>
#include <cmath>
namespace isr::package {
namespace {
[[noreturn]] void Fail(const std::string& at, const std::string& reason) {
    throw std::runtime_error("ScenePackage " + at + ": " + reason);
}
void CheckProfile(const Json& rule) {
    static const std::set<std::string> keywords={"$schema","$id","title","description","$defs","$ref","type",
        "properties","required","additionalProperties","items","minItems","maxItems","minimum","maximum",
        "minLength","enum","const","default","oneOf"};
    for (auto it=rule.begin(); it!=rule.end(); ++it) {
        if (!keywords.contains(it.key())) Fail("schema", "unsupported keyword " + it.key());
        if (it.key()=="properties" || it.key()=="$defs") for (const auto& child:it.value()) CheckProfile(child);
        if (it.key()=="items") CheckProfile(it.value());
        if (it.key()=="oneOf") for(const auto& child:it.value()) CheckProfile(child);
        if (it.key()=="$ref" && !it.value().get<std::string>().starts_with("#/$defs/")) Fail("schema","only local $defs references are supported");
    }
}
const Json& Resolve(const Json& rule) {
    return rule.contains("$ref") ? Schema().at(Json::json_pointer(rule.at("$ref").get<std::string>().substr(1))) : rule;
}
}
const Json& Schema() {
    static const Json schema=[] { auto j=Json::parse(SchemaText); CheckProfile(j); return j; }();
    return schema;
}
Json ReadJson(const std::filesystem::path& path) {
    std::ifstream file(path,std::ios::binary|std::ios::ate);
    if(!file) Fail("manifest","cannot open " + PathUtf8(path));
    const auto size=file.tellg();
    if(size<0 || size>4*1024*1024) Fail("manifest","JSON exceeds 4 MiB");
    std::string text(static_cast<size_t>(size),'\0'); file.seekg(0);
    if(!file.read(text.data(),size)) Fail("manifest","cannot read JSON");
    std::vector<std::set<std::string>> keys;
    return Json::parse(text,[&](int depth,Json::parse_event_t event,Json& parsed) {
        if(depth>64) Fail("manifest","JSON nesting exceeds 64");
        if(event==Json::parse_event_t::object_start) keys.emplace_back();
        if(event==Json::parse_event_t::key && !keys.back().insert(parsed.get<std::string>()).second) Fail("manifest","duplicate key " + parsed.get<std::string>());
        if(event==Json::parse_event_t::object_end) keys.pop_back();
        return true;
    });
}
void Validate(const Json& value,const Json& input,const std::string& at) {
    const auto& rule=Resolve(input);
    if(rule.contains("oneOf")) {
        unsigned matches=0;
        for(const auto& branch:rule["oneOf"]) { try { Validate(value,branch,at); ++matches; } catch(const std::runtime_error&) {} }
        if(matches!=1) Fail(at,"must match exactly one supported variant");
    }
    if(rule.contains("const") && value!=rule["const"]) Fail(at,"expected " + rule["const"].dump());
    if(rule.contains("enum") && std::find(rule["enum"].begin(),rule["enum"].end(),value)==rule["enum"].end()) Fail(at,"unsupported enum value");
    if(rule.contains("type")) {
        const auto type=rule["type"].get<std::string>();
        const bool valid=(type=="object"&&value.is_object()) || (type=="array"&&value.is_array()) ||
            (type=="string"&&value.is_string()) || (type=="boolean"&&value.is_boolean()) || (type=="number"&&value.is_number()) ||
            (type=="integer"&&value.is_number()&&std::isfinite(value.get<double>())&&std::floor(value.get<double>())==value.get<double>());
        if(!valid) Fail(at,"expected " + type);
    }
    if(value.is_number()) {
        const auto v=value.get<double>();
        if(!std::isfinite(v)) Fail(at,"number must be finite");
        if(rule.contains("minimum") && v<rule["minimum"].get<double>()) Fail(at,"below minimum");
        if(rule.contains("maximum") && v>rule["maximum"].get<double>()) Fail(at,"above maximum");
    }
    if(value.is_string() && rule.contains("minLength")) {
        const auto& text=value.get_ref<const std::string&>();
        const auto length=static_cast<size_t>(std::count_if(text.begin(),text.end(),[](unsigned char c){return (c&0xc0)!=0x80;}));
        if(length<rule["minLength"].get<size_t>()) Fail(at,"string is too short");
    }
    if(value.is_array()) {
        if(rule.contains("minItems")&&value.size()<rule["minItems"].get<size_t>()) Fail(at,"too few items");
        if(rule.contains("maxItems")&&value.size()>rule["maxItems"].get<size_t>()) Fail(at,"too many items");
        if(rule.contains("items")) for(size_t i=0;i<value.size();++i) Validate(value[i],rule["items"],at+"["+std::to_string(i)+"]");
    }
    if(value.is_object()) {
        for(const auto& key:rule.value("required",Json::array())) if(!value.contains(key.get<std::string>())) Fail(at,"missing " + key.get<std::string>());
        const auto properties=rule.value("properties",Json::object());
        for(auto it=value.begin();it!=value.end();++it) {
            if(properties.contains(it.key())) Validate(it.value(),properties[it.key()],at+"."+it.key());
            else if(!rule.value("additionalProperties",true)) Fail(at,"unknown field " + it.key());
        }
    }
}
void ApplyDefaults(Json& value,const Json& input) {
    const auto& rule=Resolve(input);
    if(rule.contains("oneOf")) for(const auto& branch:rule["oneOf"]) {
        try { Validate(value,branch); } catch(const std::runtime_error&) { continue; }
        ApplyDefaults(value,branch); break;
    }
    if(value.is_object()&&rule.contains("properties")) for(auto it=rule["properties"].begin();it!=rule["properties"].end();++it) {
        const auto& child=Resolve(it.value());
        if(!value.contains(it.key())&&child.contains("default")) value[it.key()]=child["default"];
        if(value.contains(it.key())) ApplyDefaults(value[it.key()],child);
    }
    if(value.is_array()&&rule.contains("items")) for(auto& item:value) ApplyDefaults(item,rule["items"]);
}
}
