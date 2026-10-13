#include "SceneNaming.hpp"
#include <set>
namespace azurerender {
std::string nextObjectName(const SceneDocument& scene,const std::string& base) {
    if(base.empty())throw std::invalid_argument("Object name base must not be empty");
    std::set<std::string> used;for(const auto& node:scene.nodes)used.insert(node.name);
    for(std::size_t number=0;number<=scene.nodes.size();++number){
        const auto name=base+std::to_string(number);if(!used.count(name))return name;
    }
    throw std::logic_error("Cannot allocate object name");
}
}
