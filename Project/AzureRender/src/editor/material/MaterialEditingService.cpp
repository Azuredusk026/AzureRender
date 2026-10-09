#include "MaterialEditingService.hpp"
#include "runtime/AssetTypeRegistry.hpp"
#include <set>
namespace azurerender {
using Json=nlohmann::json;
namespace {
Json sourceSlots(const std::filesystem::path& path){
    struct Entry {std::filesystem::file_time_type modified;std::uintmax_t size=0;Json slots;};
    static thread_local std::map<std::filesystem::path,Entry> cache;
    const auto time=std::filesystem::last_write_time(path);const auto size=std::filesystem::file_size(path);
    const auto found=cache.find(path);if(found!=cache.end()&&found->second.modified==time&&found->second.size==size)return found->second.slots;
    const auto mesh=loadGltfAsset(path.u8string());const auto identities=materialSlotIds(mesh);auto slots=Json::array();
    std::set<std::uint32_t> used;for(const auto& primitive:mesh.primitives)used.insert(primitive.materialIndex);
    for(std::size_t index=0;index<identities.size();++index)if(used.count(static_cast<std::uint32_t>(index)))slots.push_back({{"id",identities[index]},{"name",mesh.materials[index].name},{"stable",identities[index].rfind("index:",0)!=0}});
    if(cache.size()>=16)cache.erase(cache.begin());cache[path]={time,size,slots};return slots;
}
}
Json MaterialEditingService::slots(const EditorContext& context,const std::string& nodeId){
    const auto& scene=context.scene();const auto node=std::find_if(scene.nodes.begin(),scene.nodes.end(),[&](const auto& item){return item.id==nodeId;});
    if(node==scene.nodes.end())throw std::invalid_argument("Unknown material target node");
    auto result=Json::array(),diagnostics=Json::array();
    const auto resource=std::find_if(scene.resources.begin(),scene.resources.end(),[&](const auto& item){return item.id==node->resourceId;});
    if(resource!=scene.resources.end()){
        result=sourceSlots(resource->path);std::vector<std::string> identities;
        for(auto& slot:result){const auto id=slot.at("id").get<std::string>();identities.push_back(id);slot["override"]=node->materialSlots.count(id)?Json(node->materialSlots.at(id)):Json(nullptr);}
        for(const auto& [id,asset]:node->materialSlots)if(std::find(identities.begin(),identities.end(),id)==identities.end())diagnostics.push_back({{"slot",id},{"asset",asset},{"message","Source slot is missing after reimport"}});
    }
    return {{"node",nodeId},{"slots",result},{"diagnostics",diagnostics}};
}
Json MaterialEditingService::reimportSlots(const EditorContext& context,const std::string& asset){
    const auto& record=context.assets().records().at(asset);
    auto mapping=Json::array(),diagnostics=Json::array();
    if(!assetTypeRegistry().matches(record.path,{"model"}))return {{"slotMapping",mapping},{"slotDiagnostics",diagnostics}};
    const auto available=sourceSlots(record.path);
    for(const auto& slot:available)mapping.push_back({{"from",slot.at("id")},{"to",slot.at("id")},{"stable",slot.at("stable")}});
    for(const auto& node:context.scene().nodes){
        const auto resource=std::find_if(context.scene().resources.begin(),context.scene().resources.end(),[&](const auto& item){return item.id==node.resourceId;});
        if(resource==context.scene().resources.end()||resource->path!=record.path)continue;
        for(const auto& [id,material]:node.materialSlots)if(std::none_of(available.begin(),available.end(),[&](const auto& slot){return slot.at("id")==id;}))diagnostics.push_back({{"node",node.id},{"slot",id},{"asset",material},{"message","Assigned source slot is missing; clear or reassign before reimport"}});
    }
    return {{"slotMapping",mapping},{"slotDiagnostics",diagnostics}};
}
void MaterialEditingService::assign(EditorContext& context,const std::vector<std::string>& nodes,const std::string& slot,const std::string& material){
    if(!context.isProject())throw std::invalid_argument("Material slot authoring requires a project");
    if(nodes.empty()||nodes.size()>256)throw std::invalid_argument("Material target count must be 1 to 256");
    if(!material.empty())static_cast<void>(resolveMaterialBinding(material,context.assets()));
    std::set<std::string> unique;
    for(const auto& id:nodes){if(!unique.insert(id).second)throw std::invalid_argument("Duplicate material target");const auto available=slots(context,id);bool found=false;for(const auto& item:available.at("slots"))if(item.at("id")==slot)found=true;
        // A reset can clear an orphaned source slot so a reimport remains repairable.
        const auto node=std::find_if(context.scene().nodes.begin(),context.scene().nodes.end(),[&](const auto& item){return item.id==id;});
        if(!found&&( !material.empty()||node==context.scene().nodes.end()||!node->materialSlots.count(slot)))throw std::invalid_argument("Unknown material source slot");
    }
    auto candidate=context.scene();
    for(auto& node:candidate.nodes)if(unique.count(node.id)){if(material.empty())node.materialSlots.erase(slot);else node.materialSlots[slot]=material;}
    resolveSceneMaterials(candidate,context.assets());context.scene()=std::move(candidate);
}
Json MaterialEditingService::execute(EditorContext& context,ContentOperationService& content,const std::string& operation,const Json& args){
    if(!context.isProject())throw std::invalid_argument("Material authoring requires a project");
    if(operation=="material.types")return MaterialTypeRegistry::builtins().describe();
    if(operation=="material.create"){
        const auto document=MaterialDocument::defaults(args.at("type").get<std::string>());
        const auto plan=content.plan({{"operation","material.write"},{"destination",args.at("destination")},{"document",document.encode()},{"create",true}});
        const auto result=content.apply(plan.at("id").get<std::string>());context.assets().refresh();return {{"asset",context.assets().idForPath(args.at("destination").get<std::string>())},{"transaction",result}};
    }
    const auto id=args.at("asset").get<std::string>();const auto& record=context.assets().records().at(id);
    if(!assetTypeRegistry().matches(record.path,{"material"}))throw std::invalid_argument("Material operations require a material asset");
    auto document=MaterialDocument::load(record.path);auto& history=history_[id];const auto before=document.encode();
    if(history.current!=before){history.undo.clear();history.redo.clear();history.current=before;}
    if(operation=="material.describe")return {{"asset",id},{"document",before},{"type",MaterialTypeRegistry::builtins().type(document.typeId).parameters},{"textures",MaterialTypeRegistry::builtins().type(document.typeId).textures},{"canUndo",!history.undo.empty()},{"canRedo",!history.redo.empty()}};
    if(operation=="material.field"){
        const auto field=args.at("field").get<std::string>();
        if(field.rfind("textures.",0)==0){const auto slot=field.substr(9);if(args.at("value").is_null())document.textures.erase(slot);else document.textures[slot]=args.at("value");}
        else{if(!document.parameters.contains(field))throw std::invalid_argument("Unknown material parameter");document.parameters[field]=args.at("value");}
    }else if(operation=="material.undo"){
        if(history.undo.empty())throw std::invalid_argument("Material undo history is empty");document=MaterialDocument::parse(history.undo.back());
    }else if(operation=="material.redo"){
        if(history.redo.empty())throw std::invalid_argument("Material redo history is empty");document=MaterialDocument::parse(history.redo.back());
    }else if(operation!="material.save")throw std::invalid_argument("Unknown material document operation");
    const auto after=document.encode();
    for(const auto& texture:document.textures.items()){
        const auto path=context.assets().resolve(texture.value().at("asset").get<std::string>());
        if(!assetTypeRegistry().matches(path,{"image"}))throw std::invalid_argument("Material texture must reference an image asset");
        static_cast<void>(loadAssetImage(path.u8string()));
    }
    if(after!=before){
        auto validated=context.scene();resolveSceneMaterials(validated,context.assets());
        const auto plan=content.plan({{"operation","material.write"},{"destination",record.virtualPath},{"document",after},{"create",false}});content.apply(plan.at("id").get<std::string>());
        if(operation=="material.undo"){history.undo.pop_back();history.redo.push_back(before);}else if(operation=="material.redo"){history.redo.pop_back();history.undo.push_back(before);}else{history.undo.push_back(before);history.redo.clear();}
        if(history.undo.size()>128)history.undo.erase(history.undo.begin());history.current=after;
        context.assets().refresh();resolveSceneMaterials(context.scene(),context.assets());context.notifyAssetVersionChanged();
    }
    return {{"asset",id},{"document",after},{"canUndo",!history.undo.empty()},{"canRedo",!history.redo.empty()}};
}
}
