#pragma once
#include "assets/GltfLoader.hpp"
#include "assets/MaterialBinding.hpp"
#include <filesystem>
#include <functional>
#include <map>
#include <nlohmann/json.hpp>
namespace azurerender {
struct MaterialTypeDescriptor {
    std::string id,label;
    nlohmann::json parameters,textures;
    std::function<void(AssetMaterial&,const nlohmann::json&)> interpret;
};
class MaterialTypeRegistry {
public:
    void add(MaterialTypeDescriptor);
    const MaterialTypeDescriptor& type(const std::string&) const;
    nlohmann::json describe() const;
    static MaterialTypeRegistry& builtins();
private:
    std::map<std::string,MaterialTypeDescriptor> types_;
};
struct MaterialData {
    static constexpr unsigned kSchemaVersion=1;
    std::string typeId;
    nlohmann::json parameters=nlohmann::json::object(),textures=nlohmann::json::object();
    nlohmann::json authoringGraph=nullptr;
    static MaterialData defaults(const std::string&);
    static MaterialData parse(const nlohmann::json&);
    static MaterialData load(const std::filesystem::path&);
    nlohmann::json encode() const;
    void validate() const;
    AssetMaterial interpret(const std::function<std::filesystem::path(const std::string&)>&) const;
};
std::vector<std::string> materialSlotIds(const LoadedAsset&);
void applyMaterialBindings(LoadedAsset&,const std::map<std::string,MaterialBinding>&);
}
