#include "NodePresetRegistry.hpp"
#include "runtime/ComponentRegistry.hpp"
namespace azurerender {
void NodePresetRegistry::add(NodePresetDescriptor descriptor){
    if(descriptor.id.empty()||descriptor.label.empty()||!descriptor.lights.is_array())throw std::invalid_argument("Invalid node preset");
    for(const auto& component:descriptor.components)runtimeComponentRegistry().defaults(component);
    for(const auto& light:descriptor.lights){
        const auto color=light.at("color").get<std::array<float,3>>();for(float c:color)if(!std::isfinite(c)||c<0)throw std::invalid_argument("Invalid preset light color");
        const auto radius=light.at("radius").get<float>(),intensity=light.at("intensity").get<float>();
        if(!std::isfinite(radius)||radius<=0||!std::isfinite(intensity)||intensity<0)throw std::invalid_argument("Invalid preset light ranges");
    }
    const auto id=descriptor.id;if(!entries_.emplace(id,std::move(descriptor)).second)throw std::invalid_argument("Duplicate node preset");
}
nlohmann::json NodePresetRegistry::describe() const{auto result=nlohmann::json::array();for(const auto& [id,preset]:entries_)result.push_back({{"id",id},{"label",preset.label},{"components",preset.components},{"lights",preset.lights}});return result;}
void NodePresetRegistry::create(EditorContext& context,const std::string& id,const std::string& requested) const{
    const auto found=entries_.find(id);if(found==entries_.end())throw std::invalid_argument("Unknown node preset");
    const auto& preset=found->second;context.createNode(requested);context.setSelectedNodeName(preset.label);
    for(const auto& component:preset.components)context.addGameplayComponent(component);
    const auto node=context.selectedNode()->id;std::size_t index=0;
    for(const auto& light:preset.lights)context.scene().lights.push_back({node+":light-"+std::to_string(index++),node,light.at("color").get<std::array<float,3>>(),light.at("intensity").get<float>(),light.at("radius").get<float>(),light.value("enabled",true)});
}
NodePresetRegistry NodePresetRegistry::builtins(){
    NodePresetRegistry result;result.add({"camera","Third-person camera",{"azure.third-person-camera"}});
    result.add({"light","Point light",{},nlohmann::json::array({{{"color",{1,1,1}},{"intensity",2},{"radius",8},{"enabled",true}}})});return result;
}
}
