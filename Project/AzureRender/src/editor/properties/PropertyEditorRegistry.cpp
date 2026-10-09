#include "PropertyEditorRegistry.hpp"
#include "runtime/ComponentRegistry.hpp"
#include <algorithm>
#include <set>
namespace azurerender {
const reflection::Property& PropertyEditorRegistry::field(const std::string& type,const std::string& name) {
    const auto& fields=runtimeComponentRegistry().metadata().type(type).properties;
    const auto found=std::find_if(fields.begin(),fields.end(),[&](const auto& value){return value.name==name;});
    if(found==fields.end())throw std::invalid_argument("Unknown component field: "+name);
    return *found;
}
template<class View>
static nlohmann::json selectionFields(const View& context,const std::vector<std::string>& nodes,const std::string& type) {
    auto result=nlohmann::json::object();if(nodes.empty())return result;
    for(const auto& property:runtimeComponentRegistry().metadata().type(type).properties) {
        if(!property.toolVisible)continue;
        nlohmann::json value;bool first=true,mixed=false;std::array<bool,3> mixedAxes{};
        for(const auto& id:nodes) {
            const auto data=context.componentData(id,type);if(data.is_null())return nlohmann::json::object();
            const auto current=data.at(property.name);
            if(first){value=current;first=false;}else {
                mixed=mixed||value!=current;
                if(property.kind==reflection::Kind::Vector3)for(std::size_t axis=0;axis<3;++axis)mixedAxes[axis]=mixedAxes[axis]||value[axis]!=current[axis];
            }
        }
        result[property.name]={{"value",value},{"mixed",mixed},{"mixedAxes",mixedAxes},{"unit",property.unit},
            {"precision",property.precision},{"editable",!property.readOnly&&(nodes.size()==1||property.batchEditable)}};
    }
    return result;
}
nlohmann::json PropertyEditorRegistry::selection(const EditorContext& context,const std::vector<std::string>& nodes,const std::string& type){return selectionFields(context,nodes,type);}
nlohmann::json PropertyEditorRegistry::selection(const PanelDocumentView& context,const std::vector<std::string>& nodes,const std::string& type){return selectionFields(context,nodes,type);}
void PropertyEditorRegistry::apply(EditorContext& candidate,const nlohmann::json& args) {
    const auto ids=args.at("nodes").get<std::vector<std::string>>();
    if(ids.empty()||ids.size()>4096)throw std::invalid_argument("Batch selection exceeds its budget");
    const auto type=args.at("type").get<std::string>(),name=args.at("field").get<std::string>();
    const auto& property=field(type,name);
    if(property.readOnly||!property.toolVisible||(ids.size()>1&&!property.batchEditable))throw std::invalid_argument("Field does not permit this batch edit");
    const int axis=args.value("axis",-1);
    if(args.contains("axis")&&(property.kind!=reflection::Kind::Vector3||axis<0||axis>2))throw std::invalid_argument("Invalid vector axis");
    const auto original=candidate.selectedNodes();std::set<std::string> seen;
    std::vector<std::pair<std::size_t,nlohmann::json>> changes;
    for(const auto& id:ids) {
        const auto& nodes=candidate.scene().nodes;
        const auto node=std::find_if(nodes.begin(),nodes.end(),[&](const auto& value){return value.id==id;});
        if(node==nodes.end()||!seen.insert(id).second)throw std::invalid_argument("Invalid batch target: "+id);
        const auto data=candidate.componentData(id,type);
        if(data.is_null())throw std::invalid_argument("Batch target lacks component: "+id);
        auto value=args.value("reset",false)?runtimeComponentRegistry().defaults(type).at("data").at(name):args.at("value");
        if(args.value("reset",false)&&property.referenceDefault=="selected-node")value=id;
        if(axis>=0){auto vector=data.at(name);vector[axis]=args.value("reset",false)?value[axis]:value;value=std::move(vector);}
        runtimeComponentRegistry().validateWrite(type,name,value);
        changes.emplace_back(static_cast<std::size_t>(node-nodes.begin()),std::move(value));
    }
    for(const auto& [index,value]:changes){candidate.selectNode(index);candidate.setComponentField(type,name,value);}
    candidate.selectNodes(original);
}
}
