#pragma once
#include "editor/EditorContext.hpp"
#include <functional>
namespace azurerender {
struct NodePresetDescriptor {
    std::string id,label;std::vector<std::string> components;
    nlohmann::json lights=nlohmann::json::array();
    std::string geometry,nameBase;
    std::array<float,3> dimensions{1,1,1};
    nlohmann::json componentValues=nlohmann::json::object();
};
class NodePresetRegistry {
public:
    void add(NodePresetDescriptor);
    nlohmann::json describe() const;
    void create(EditorContext&,const std::string& preset,const std::string& id,const std::string& parent={}) const;
    static NodePresetRegistry builtins();
private:
    std::map<std::string,NodePresetDescriptor> entries_;
};
}
