#pragma once
#include "runtime/AssetDatabase.hpp"
#include "runtime/ComponentCodec.hpp"
namespace azurerender {
class Level {
public:
    SceneDocument scene;
    std::map<std::string, nlohmann::json> components;
    static Level load(const std::filesystem::path& file, const AssetDatabase& assets);
    static Level parse(const nlohmann::json& document, const AssetDatabase& assets);
    void save(const std::filesystem::path& path) const;
    void setComponent(const std::string& node, const std::string& type, const nlohmann::json& envelope, const AssetDatabase& assets);
private:
    nlohmann::json document_;
};
} // namespace azurerender
