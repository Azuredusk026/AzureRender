#include "SelectionService.hpp"
#include "EditorContext.hpp"
#include "commands/EditService.hpp"
#include <algorithm>
#include <set>
namespace azurerender {
void SelectionService::set(const std::vector<std::string>& ids) {
    const auto& nodes=context_.scene().nodes;std::vector<std::size_t> indices;std::set<std::string> seen;
    for(const auto& id:ids) {
        const auto node=std::find_if(nodes.begin(),nodes.end(),[&](const auto& candidate){return candidate.id==id;});
        if(node==nodes.end()||!seen.insert(id).second)throw std::invalid_argument("Invalid selection identity: "+id);
        indices.push_back(static_cast<std::size_t>(node-nodes.begin()));
    }
    const auto result=edits_.current("node.select",{{"indices",indices}});if(!result)throw std::runtime_error(result.diagnostics.dump());
}
std::vector<std::string> SelectionService::selected() const {
    std::vector<std::string> result;const auto& nodes=context_.scene().nodes;
    for(const auto index:context_.selectedNodes())if(index<nodes.size())result.push_back(nodes[index].id);
    return result;
}
}
