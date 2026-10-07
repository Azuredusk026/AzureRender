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
    anchor_=ids.empty()?std::string():ids.back();reveal_=true;
}
std::string SelectionService::active() const{return context_.selectedNode()?context_.selectedNode()->id:std::string();}
void SelectionService::click(const std::string& id,bool ctrl,bool shift,const std::vector<std::string>& visible){
    if(id.empty()){if(!ctrl)set({});return;}
    const auto& nodes=context_.scene().nodes;
    if(std::none_of(nodes.begin(),nodes.end(),[&](const auto& node){return node.id==id;}))throw std::invalid_argument("Unknown selection identity");
    auto ids=ctrl?selected():std::vector<std::string>{};
    if(shift){
        std::set<std::string> seen;
        for(const auto& entry:visible)if(!seen.insert(entry).second||std::none_of(nodes.begin(),nodes.end(),[&](const auto& node){return node.id==entry;}))throw std::invalid_argument("Invalid visible selection order");
        const auto end=std::find(visible.begin(),visible.end(),id);if(end==visible.end())throw std::invalid_argument("Range target is not visible");
        auto start=std::find(visible.begin(),visible.end(),anchor_);if(start==visible.end())start=end;
        const auto low=std::min(start,end),high=std::max(start,end);
        for(auto entry=low;entry<=high;++entry)if(std::find(ids.begin(),ids.end(),*entry)==ids.end())ids.push_back(*entry);
        ids.erase(std::remove(ids.begin(),ids.end(),id),ids.end());ids.push_back(id);
        const auto anchor=anchor_;set(ids);anchor_=anchor.empty()?id:anchor;
    }else if(ctrl){
        const auto found=std::find(ids.begin(),ids.end(),id);if(found==ids.end())ids.push_back(id);else ids.erase(found);set(ids);anchor_=id;
    }else set({id});
}
std::vector<std::string> SelectionService::selected() const {
    std::vector<std::string> result;const auto& nodes=context_.scene().nodes;
    for(const auto index:context_.selectedNodes())if(index<nodes.size())result.push_back(nodes[index].id);
    return result;
}
}
