#include "NodePresetRegistry.hpp"
#include "runtime/ComponentRegistry.hpp"
#include "BuiltinGeometryService.hpp"
#include "SceneNaming.hpp"
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
void NodePresetRegistry::create(EditorContext& context,const std::string& id,const std::string& requested,const std::string& parent) const{
    const auto found=entries_.find(id);if(found==entries_.end())throw std::invalid_argument("Unknown node preset");
    if(!parent.empty()&&std::none_of(context.scene().nodes.begin(),context.scene().nodes.end(),[&](const auto& n){return n.id==parent;}))throw std::invalid_argument("Unknown creation parent");
    const auto& preset=found->second;const auto name=nextObjectName(context.scene(),preset.nameBase.empty()?preset.label:preset.nameBase);
    if(preset.geometry.empty())context.createNode(requested);
    else context.placeResource(ensureBuiltinGeometry(context,preset.geometry,preset.dimensions),requested);
    context.setSelectedNodeName(name);
    for(const auto& component:preset.components)context.addGameplayComponent(component);
    for(const auto& values:preset.componentValues.items())context.setComponentFields(values.key(),values.value());
    if(!parent.empty())context.reparentNode(context.selectedNode()->id,parent);
    const auto node=context.selectedNode()->id;std::size_t index=0;
    for(const auto& light:preset.lights)context.scene().lights.push_back({node+":light-"+std::to_string(index++),node,light.at("color").get<std::array<float,3>>(),light.at("intensity").get<float>(),light.at("radius").get<float>(),light.value("enabled",true)});
}
NodePresetRegistry NodePresetRegistry::builtins(){
    NodePresetRegistry result;result.add({"empty","Empty",{}});
    result.add({"camera","Camera",{"azure.camera"}});
    result.add({"follow-camera","Third Person Camera",{"azure.third-person-camera"}});
    result.add({"game-ui","Game UI",{"azure.game-ui"}});
    result.add({"light","Point Light",{},nlohmann::json::array({{{"color",{1,1,1}},{"intensity",2},{"radius",8},{"enabled",true}}})});
    for(const auto& pair:std::vector<std::pair<std::string,std::string>>{{"cube","Cube"},{"plane","Plane"},{"sphere","Sphere"},{"cylinder","Cylinder"},{"capsule","Capsule"},{"torus","Torus"}}){
        NodePresetDescriptor preset; preset.id=pair.first;preset.label=pair.second;preset.nameBase=pair.second;preset.geometry=pair.first;
        if(pair.first=="capsule")preset.dimensions={1,2,1};if(pair.first=="torus")preset.dimensions={1,.25F,1};
        if(pair.first!="torus"){
            preset.components={"azure.rigid-body"};
            const auto shape=pair.first=="sphere"?"sphere":pair.first=="capsule"?"capsule":"box";
            preset.componentValues["azure.rigid-body"]={{"shape",shape},{"dynamic",false},{"halfExtent",{.5F,pair.first=="plane"?.05F:.5F,.5F}},{"radius",.5F},{"halfHeight",.5F}};
        }result.add(std::move(preset));
    }return result;
}
}
