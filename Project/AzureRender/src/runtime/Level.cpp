#include "runtime/Level.hpp"
#include "runtime/LevelRenderSettings.hpp"
#include "runtime/Prefab.hpp"
#include "runtime/RuntimeLifecycle.hpp"
#include <fstream>
#include <chrono>
#include <atomic>
#include <set>
namespace azurerender {
Level Level::load(const std::filesystem::path& path, const AssetDatabase& assets) {
    std::ifstream input(path); if (!input) throw std::runtime_error("Cannot open level: " + path.string());
    nlohmann::json document; input >> document; return parse(document, assets);
}
Level Level::parse(const nlohmann::json& document, const AssetDatabase& assets) {
    const auto expanded = expandPrefabs(document, assets);
    Level level; level.document_ = document;
    level.reloadKey_ = expanded.dump();
    level.scene.sceneId = expanded.at("id").get<std::string>();
    if (level.scene.sceneId.empty()) throw std::runtime_error("Level id must be nonempty");
    const auto sceneType = expanded.value("sceneType", "character");
    if (sceneType == "character") level.scene.renderSettings.sceneType = SceneType::Character;
    else if (sceneType == "sample") level.scene.renderSettings.sceneType = SceneType::Sample;
    else if (sceneType == "blackhole") level.scene.renderSettings.sceneType = SceneType::Blackhole;
    else throw std::runtime_error("Unknown level sceneType");
    if(expanded.contains("renderSettings"))decodeLevelRenderSettings(level.scene.renderSettings,expanded.at("renderSettings"));
    std::set<std::string> resources;
    for (const auto& source : expanded.at("resources")) {
        SceneResource resource{source.at("id").get<std::string>(), "gltf", assets.resolveReference(source.at("asset").get<std::string>())};
        if (resource.id.empty() || !resources.insert(resource.id).second) throw std::runtime_error("Duplicate level resource");
        level.scene.resources.push_back(std::move(resource));
        for (const auto& record : assets.records())
            if (record.second.path == level.scene.resources.back().path)
                level.reloadKey_ += ":" + std::to_string(record.second.fingerprint);
    }
    const auto registry = reflection::makeRuntimeRegistry();
    for (const auto& source : expanded.at("nodes")) {
        SceneNode node; node.id = source.at("id").get<std::string>(); node.name = source.value("name", node.id);
        node.parentId = source.value("parentId", ""); node.resourceId = source.value("resourceId", "");
        node.prefabSource = source.value("prefabSource", ""); node.instanceOf = source.value("instanceOf", "");
        if (!node.resourceId.empty() && !resources.count(node.resourceId)) throw std::runtime_error("Node references unknown resource");
        node.visible = !node.resourceId.empty();
        auto data = source.value("components", nlohmann::json::object()); validateComponents(data);
        if (data.contains("azure.transform")) {
            ecs::TransformComponent transform; registry.decode("azure.transform", &transform, data.at("azure.transform"));
            node.translation = transform.translation; node.rotation = transform.rotation; node.scale = transform.scale;
        }
        if (data.contains("azure.renderable")) {
            ecs::RenderableComponent renderable; registry.decode("azure.renderable", &renderable, data.at("azure.renderable"));
            node.visible = renderable.visible && !node.resourceId.empty();
        }
        if (!level.components.emplace(node.id, std::move(data)).second) throw std::runtime_error("Duplicate level node");
        level.scene.nodes.push_back(std::move(node));
    }
    std::set<std::string> lights;
    for (const auto& source : expanded.value("lights", nlohmann::json::array())) {
        SceneLight light; light.id = source.at("id").get<std::string>(); light.nodeId = source.at("nodeId").get<std::string>();
        light.color = source.value("color", light.color); light.intensity = source.value("intensity", light.intensity);
        light.radius = source.value("radius", light.radius); light.enabled = source.value("enabled", light.enabled);
        if (!lights.insert(light.id).second || !level.components.count(light.nodeId) || !std::isfinite(light.intensity) || light.intensity < 0
            || !std::isfinite(light.radius) || light.radius <= 0) throw std::runtime_error("Invalid level light");
        level.scene.lights.push_back(std::move(light));
    }
    for(const auto& resource:level.scene.resources){
        level.resourceKey+=resource.path.generic_string()+":";
        for(const auto& record:assets.records())if(record.second.path==resource.path)level.resourceKey+=std::to_string(record.second.fingerprint);
    }
    RuntimeLifecycle validation; validation.loadScene(level.scene);
    return level;
}
void Level::save(const std::filesystem::path& path) const {
    static std::atomic<std::uint64_t> sequence{0};
    const auto temporary = path.string() + ".tmp."
        + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())
        + "." + std::to_string(sequence++);
    try {
        { std::ofstream output(temporary, std::ios::binary);
          output << document_.dump(2) << '\n'; output.flush();
          if (!output) throw std::runtime_error("Cannot save complete level"); }
        std::filesystem::rename(temporary, path);
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        throw;
    }
}
void Level::setComponent(const std::string& node, const std::string& type, const nlohmann::json& envelope, const AssetDatabase& assets) {
    auto document = document_;
    if (!components.count(node)) throw std::runtime_error("Unknown level node: " + node);
    bool found = false;
    for (auto& source : document["nodes"]) if (source.at("id") == node) { source["components"][type] = envelope; found = true; }
    if (!found) for (auto& instance : document["prefabs"]) {
        const auto prefix = instance.at("instance").get<std::string>() + ":";
        if (node.rfind(prefix, 0) == 0) { instance["overrides"][node.substr(prefix.size())]["components"][type] = envelope; found = true; break; }
    }
    if (!found) throw std::runtime_error("Cannot locate serialized node");
    *this = parse(document, assets);
}
} // namespace azurerender
