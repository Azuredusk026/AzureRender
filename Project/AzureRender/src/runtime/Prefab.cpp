#include "runtime/Prefab.hpp"
#include "runtime/ComponentRegistry.hpp"
#include <fstream>
#include <set>
#include <stdexcept>
namespace azurerender {
namespace {
using Json = nlohmann::json;
Json expand(const Json& document, const AssetDatabase& assets, std::set<std::string>& stack,const std::map<std::string,Json>& sources) {
    if (!document.at("schemaVersion").is_number_integer() || document.at("schemaVersion") != 1) throw std::runtime_error("Unsupported level/prefab version");
    Json result = document;
    if (!result.contains("nodes")) result["nodes"] = Json::array();
    if (!result.contains("resources")) result["resources"] = Json::array();
    if (!result["nodes"].is_array() || !result["resources"].is_array()) throw std::runtime_error("Nodes and resources must be arrays");
    std::set<std::string> instances;
    for (const auto& instance : document.value("prefabs", Json::array())) {
        const auto asset = instance.at("asset").get<std::string>(), prefix = instance.at("instance").get<std::string>();
        if (prefix.empty() || prefix.find(':') != std::string::npos || !instances.insert(prefix).second)
            throw std::runtime_error("Invalid or duplicate prefab instance");
        if (!stack.insert(asset).second) throw std::runtime_error("Prefab reference cycle: " + asset);
        Json prefab;const auto replacement=sources.find(asset);
        if(replacement!=sources.end())prefab=replacement->second;else{std::ifstream file(assets.resolveReference(asset));file>>prefab;}
        prefab = expand(prefab, assets, stack,sources); stack.erase(asset);
        const auto overrides = instance.value("overrides", Json::object());
        if (!overrides.is_object()) throw std::runtime_error("Prefab overrides must be object");
        std::set<std::string> applied;
        std::map<std::string,std::string> mapping;for(const auto& node:prefab["nodes"]){const auto local=node.at("id").get<std::string>();mapping[local]=prefix+":"+local;}
        for (auto node : prefab["nodes"]) {
            const auto local = node.at("id").get<std::string>();
            if (overrides.contains(local)) { node.merge_patch(overrides.at(local)); applied.insert(local); }
            if (node.at("id") != local) throw std::runtime_error("Prefab override cannot change stable node id");
            node["id"] = prefix + ":" + local;
            for (const auto* link : {"parentId", "resourceId"})
                if (!node.value(link, std::string()).empty()) node[link] = prefix + ":" + node.at(link).get<std::string>();
            remapPrefabReferences(node,mapping);
            node["prefabSource"] = asset; node["instanceOf"] = local;
            result["nodes"].push_back(std::move(node));
        }
        if (applied.size() != overrides.size()) throw std::runtime_error("Prefab override targets an unknown node");
        for (auto resource : prefab["resources"]) {
            resource["id"] = prefix + ":" + resource.at("id").get<std::string>();
            result["resources"].push_back(std::move(resource));
        }
        for (auto light : prefab.value("lights", Json::array())) {
            light["id"] = prefix + ":" + light.at("id").get<std::string>();
            light["nodeId"] = prefix + ":" + light.at("nodeId").get<std::string>();
            if (!result.contains("lights")) result["lights"] = Json::array();
            result["lights"].push_back(std::move(light));
        }
    }
    result.erase("prefabs"); return result;
}
}
nlohmann::json expandPrefabs(const nlohmann::json& document, const AssetDatabase& assets,const std::map<std::string,Json>& sources) {
    std::set<std::string> stack; return expand(document, assets, stack,sources);
}
void remapPrefabReferences(Json& node,const std::map<std::string,std::string>& mapping){
    if(!node.contains("components"))return;
    for(auto& component:node["components"].items())for(const auto& field:runtimeComponentRegistry().metadata().type(component.key()).properties){
        if(field.reference!="node"||!component.value().at("data").contains(field.name))continue;
        auto& reference=component.value()["data"][field.name];const auto found=mapping.find(reference.get<std::string>());if(found!=mapping.end())reference=found->second;
    }
}
Json canonicalPrefabNode(Json node){
    const auto id=node.at("id").get<std::string>();node["name"]=node.value("name",id);node["parentId"]=node.value("parentId",std::string());node["resourceId"]=node.value("resourceId",std::string());
    node["materialSlots"]=node.value("materialSlots",Json::object());node.erase("prefabSource");node.erase("instanceOf");
    auto components=node.value("components",Json::object());
    if(!components.contains("azure.transform"))components["azure.transform"]=runtimeComponentRegistry().defaults("azure.transform");
    if(!components.contains("azure.renderable")){components["azure.renderable"]=runtimeComponentRegistry().defaults("azure.renderable");components["azure.renderable"]["data"]["visible"]=!node["resourceId"].get<std::string>().empty();}
    ecs::World world;const auto entity=world.createEntity();
    for(auto& component:components.items()){runtimeComponentRegistry().install(component.key(),world,entity,component.value());component.value()=runtimeComponentRegistry().encode(component.key(),world,entity);}
    node["components"]=std::move(components);return node;
}
Json prefabOverride(const Json& base,const Json& current){
    if(!base.is_object()||!current.is_object())return current;
    auto patch=Json::object();
    for(const auto& field:base.items())if(!current.contains(field.key()))patch[field.key()]=nullptr;
    for(const auto& field:current.items())if(!base.contains(field.key()))patch[field.key()]=field.value();else if(base.at(field.key())!=field.value())patch[field.key()]=prefabOverride(base.at(field.key()),field.value());
    return patch;
}
} // namespace azurerender
