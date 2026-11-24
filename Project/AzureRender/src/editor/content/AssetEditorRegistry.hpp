#pragma once
#include <map>
#include <nlohmann/json.hpp>
namespace azurerender {
struct AssetEditorDescriptor {std::string type,label,openOperation;};
class AssetEditorRegistry {
public:
    void add(AssetEditorDescriptor);
    nlohmann::json open(const std::string& type,const std::string& asset) const;
    static AssetEditorRegistry builtins();
private:
    std::map<std::string,AssetEditorDescriptor> entries_;
};
}
