#pragma once
#include "editor/EditorContext.hpp"
#include <functional>
namespace azurerender {
struct NodePresetDescriptor {std::string id,label;std::vector<std::string> components;nlohmann::json lights=nlohmann::json::array();};
class NodePresetRegistry {
public:
    void add(NodePresetDescriptor);
    nlohmann::json describe() const;
    void create(EditorContext&,const std::string& preset,const std::string& id) const;
    static NodePresetRegistry builtins();
private:
    std::map<std::string,NodePresetDescriptor> entries_;
};
}
