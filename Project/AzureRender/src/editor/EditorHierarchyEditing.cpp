#include "editor/EditorContext.hpp"
#include "runtime/ComponentRegistry.hpp"
#include "runtime/AssetTypeRegistry.hpp"
#include "scene/TransformEditing.hpp"
#include "scene/SceneDescription.hpp"
#include <set>
namespace azurerender {
void EditorContext::placeAssetAt(const std::string& asset,const std::array<float,3>& position) {
    for(const auto value:position)if(!std::isfinite(value))throw std::invalid_argument("Placement position must be finite");
    std::filesystem::path path;
    const auto found=std::find_if(scene_.resources.begin(),scene_.resources.end(),[&](const auto& resource){return resource.id==asset;});
    if(found!=scene_.resources.end())path=resolvedResourcePath(*found);else if(assets_)path=assets_->resolveReference(asset);
    const auto* type=assetTypeRegistry().classify(path);if(!type||!type->placeable)throw std::invalid_argument("This asset type cannot be placed in a scene");
    if(type->id=="prefab"){
        std::set<std::string> existing;for(const auto& node:scene_.nodes)existing.insert(node.id);
        placePrefab(asset,"prefab-"+newDocumentIdentity());
        for(auto& node:scene_.nodes)if(!existing.count(node.id)&&node.parentId.empty())node.translation=internal::addVectors(node.translation,position);
        rebuildEntities();refreshSelectedTransform();
    }else{placeResource(asset);setGizmoTranslation(position);}
}
void EditorContext::reparentNode(const std::string& id,const std::string& parent) {
    const auto find=[&](const std::string& identity){return std::find_if(scene_.nodes.begin(),scene_.nodes.end(),[&](const auto& node){return node.id==identity;});};
    const auto node=find(id);if(node==scene_.nodes.end())throw std::invalid_argument("Unknown node: "+id);
    if(!parent.empty()&&find(parent)==scene_.nodes.end())throw std::invalid_argument("Unknown parent: "+parent);
    std::set<std::string> seen;auto ancestor=parent;
    while(!ancestor.empty()){
        if(ancestor==id||!seen.insert(ancestor).second)throw std::invalid_argument("Reparent would create a hierarchy cycle");
        const auto current=find(ancestor);if(current==scene_.nodes.end())throw std::invalid_argument("Missing ancestor");ancestor=current->parentId;
    }
    if(node->parentId==parent)return;
    const auto worlds=scene::resolveNodeWorldTransforms(scene_.renderDescription());
    auto local=worlds.at(static_cast<std::size_t>(node-scene_.nodes.begin()));
    if(!parent.empty())local=internal::multiply(scene::inverseAffine(worlds.at(static_cast<std::size_t>(find(parent)-scene_.nodes.begin()))),local);
    const auto transform=scene::decomposeTrs(local,node->scale);
    beginEdit();node->parentId=parent;node->translation=transform.translation;node->rotation=transform.rotation;node->scale=transform.scale;
    rebuildEntities();refreshSelectedTransform();
}
void EditorContext::removeGameplayComponent(const std::string& type) {
    if(!selectedNode())throw std::invalid_argument("Component removal requires a selection");
    if(type=="azure.transform"||type=="azure.renderable")throw std::invalid_argument("This component is required by the scene node");
    runtimeComponentRegistry().metadata().type(type);
    auto data=runtimeComponents().at(selectedNode()->id);if(!data.contains(type))throw std::invalid_argument("Node lacks component: "+type);
    data.erase(type);validateComponentReferences(*selectedNode(),data,scene_);beginEdit();components_[selectedNode()->id]=std::move(data);rebuildEntities();
}
void EditorContext::resetComponentField(const std::string& type,const std::string& name) {
    if(!selectedNode())throw std::invalid_argument("Field reset requires a selection");
    const auto& properties=runtimeComponentRegistry().metadata().type(type).properties;
    const auto field=std::find_if(properties.begin(),properties.end(),[&](const auto& value){return value.name==name;});
    if(field==properties.end())throw std::invalid_argument("Unknown component field: "+name);
    auto value=runtimeComponentRegistry().defaults(type).at("data").at(name);
    if(field->referenceDefault=="selected-node")value=selectedNode()->id;
    setComponentField(type,name,value);
}
}
