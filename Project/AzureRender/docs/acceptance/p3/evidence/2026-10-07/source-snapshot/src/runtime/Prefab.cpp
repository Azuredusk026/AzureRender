#include "runtime/Prefab.hpp"
#include <fstream>
#include <set>
#include <stdexcept>
namespace azurerender {
namespace {
using Json = nlohmann::json;
Json expand(const Json& document, const AssetDatabase& assets, std::set<std::string>& stack) {
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
        std::ifstream file(assets.resolveReference(asset)); Json prefab; file >> prefab;
        prefab = expand(prefab, assets, stack); stack.erase(asset);
        const auto overrides = instance.value("overrides", Json::object());
        if (!overrides.is_object()) throw std::runtime_error("Prefab overrides must be object");
        std::set<std::string> applied;
        for (auto node : prefab["nodes"]) {
            const auto local = node.at("id").get<std::string>();
            if (overrides.contains(local)) { node.merge_patch(overrides.at(local)); applied.insert(local); }
            if (node.at("id") != local) throw std::runtime_error("Prefab override cannot change stable node id");
            node["id"] = prefix + ":" + local;
            for (const auto* link : {"parentId", "resourceId"})
                if (!node.value(link, std::string()).empty()) node[link] = prefix + ":" + node.at(link).get<std::string>();
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
nlohmann::json expandPrefabs(const nlohmann::json& document, const AssetDatabase& assets) {
    std::set<std::string> stack; return expand(document, assets, stack);
}
} // namespace azurerender
