#include "PrefabAuthoringService.hpp"
#include "editor/EditorSession.hpp"
#include "editor/documents/DocumentBatchTransaction.hpp"
#include "runtime/Prefab.hpp"
#include <fstream>
#include <set>
namespace azurerender {
namespace {
using Json=nlohmann::json;
Json read(const std::filesystem::path& path){if(std::filesystem::file_size(path)>2097152)throw std::invalid_argument("Prefab source exceeds authoring budget");std::ifstream input(path);Json result;input>>result;return result;}
Json instanceRow(const EditorContext& context,const std::string& id){for(const auto& row:context.levelDocument().value("prefabs",Json::array()))if(row.at("instance")==id)return row;throw std::invalid_argument("Unknown prefab instance");}
Json nodes(const Json& document){auto result=Json::object();for(const auto& node:document.at("nodes"))result[node.at("id").get<std::string>()]=canonicalPrefabNode(node);return result;}
Json fieldsFor(const Json& base,const Json& current){auto fields=Json::array();for(const auto& item:current.items()){
    if(!base.contains(item.key()))throw std::invalid_argument("Prefab source topology changed");
    for(const auto& operation:Json::diff(base.at(item.key()),item.value()))fields.push_back({{"node",item.key()},{"path",operation.at("path")},{"operation",operation}});
}return fields;}
void copyFields(Json& target,const Json& current,const Json& fields,const Json& allowed){
    if(!fields.is_array()||fields.empty()||fields.size()>128)throw std::invalid_argument("Prefab field range requires 1 to 128 entries");std::set<std::pair<std::string,std::string>> unique;
    for(const auto& field:fields){if(!field.is_object()||field.size()!=2)throw std::invalid_argument("Prefab fields require node and JSON pointer");const auto node=field.at("node").get<std::string>(),path=field.at("path").get<std::string>();
        if(path.empty()||path=="/id"||path.rfind("/id/",0)==0||!unique.emplace(node,path).second)throw std::invalid_argument("Invalid or duplicate prefab field range");
        bool found=false;for(const auto& candidate:allowed)if(candidate.at("node")==node&&candidate.at("path")==path)found=true;if(!found)throw std::invalid_argument("Prefab field is outside the override list");
        const Json::json_pointer pointer(path);const auto& value=current.at(node);
        if(value.contains(pointer))target[node][pointer]=value.at(pointer);else target[node]=target.at(node).patch(Json::array({{{"op","remove"},{"path",path}}}));
    }
}
Json localNodes(EditorContext& context,const std::string& prefix){
    auto expanded=expandPrefabs(context.levelDocument(),context.assets());auto result=Json::object();std::map<std::string,std::string> mapping;
    for(const auto& node:expanded.at("nodes")){const auto id=node.at("id").get<std::string>();if(id.rfind(prefix+":",0)==0)mapping[id]=id.substr(prefix.size()+1);}
    for(auto node:expanded.at("nodes")){const auto id=node.at("id").get<std::string>();if(!mapping.count(id))continue;
        node["id"]=mapping.at(id);for(const auto* field:{"parentId","resourceId"}){const auto link=node.value(field,std::string());if(!link.empty()){if(link.rfind(prefix+":",0)!=0)throw std::invalid_argument("Prefab link leaves its instance");node[field]=link.substr(prefix.size()+1);}}
        remapPrefabReferences(node,mapping);result[mapping.at(id)]=canonicalPrefabNode(node);
    }return result;
}
}
struct PrefabAuthoringService::Impl {
    struct Document {std::string id;EditorContext::Snapshot before;DocumentVersion after;EditorContext::Snapshot applied;};
    struct History {std::string asset,path;Json sourceBefore,sourceAfter;std::vector<Document> documents;};
    std::vector<History> history,redo;
};
PrefabAuthoringService::PrefabAuthoringService():impl_(std::make_unique<Impl>()){}
PrefabAuthoringService::~PrefabAuthoringService()=default;
void PrefabAuthoringService::clear(){impl_->history.clear();impl_->redo.clear();}
void PrefabAuthoringService::adopt(EditorContext& context,const Json& document,const std::map<std::string,Json>& sources){
    auto level=Level::parse(document,context.assets(),sources);std::vector<std::string> selected;for(const auto index:context.selectedNodes_)selected.push_back(context.scene_.nodes.at(index).id);
    for(const auto& node:level.scene.nodes)context.validateComponentReferences(node,level.components.at(node.id),level.scene);
    context.scene_=std::move(level.scene);context.components_=std::move(level.components);context.sourceLevel_=document;context.capturePrefabSources(sources);
    const auto expanded=expandPrefabs(document,context.assets(),sources);for(const auto& resource:expanded.at("resources")){const auto asset=resource.at("asset").get<std::string>();context.resourceReferences_[context.assets().resolveReference(asset)]=asset;}
    context.selectedNodes_.clear();for(const auto& id:selected)for(std::size_t index=0;index<context.scene_.nodes.size();++index)if(context.scene_.nodes[index].id==id)context.selectedNodes_.push_back(index);
    context.selectedNodeIndex_=context.selectedNodes_.empty()?0:context.selectedNodes_.back();context.rebuildEntities();context.refreshSelectedTransform();
}
Json PrefabAuthoringService::create(EditorSession& session,const std::vector<std::string>& selected,const std::string& path){
    auto& context=session.context();if(!context.isProject()||selected.empty()||selected.size()>256)throw std::invalid_argument("Prefab creation requires 1 to 256 project objects");
    std::set<std::string> included(selected.begin(),selected.end());if(included.size()!=selected.size())throw std::invalid_argument("Prefab creation requires unique roots");
    const auto expanded=expandPrefabs(context.levelDocument(),context.assets());const auto all=nodes(expanded);
    for(const auto& id:included)if(!all.contains(id))throw std::invalid_argument("Unknown prefab creation root");
    bool changed=true;while(changed){changed=false;for(const auto& node:all.items())if(included.count(node.value().at("parentId").get<std::string>())&&!included.count(node.key())){included.insert(node.key());changed=true;}}
    if(included.size()>256)throw std::invalid_argument("Prefab descendants exceed authoring budget");
    Json document={{"schemaVersion",1},{"id",std::filesystem::u8path(path).stem().u8string()},{"nodes",Json::array()},{"resources",Json::array()},{"lights",Json::array()}};std::set<std::string> resources;
    for(const auto& source:expanded.at("nodes")){const auto id=source.at("id").get<std::string>();if(!included.count(id))continue;auto node=canonicalPrefabNode(source);
        if(!node.at("parentId").get<std::string>().empty()&&!included.count(node.at("parentId").get<std::string>()))throw std::invalid_argument("Prefab root depends on an external parent");
        for(const auto& component:node.at("components").items())for(const auto& field:runtimeComponentRegistry().metadata().type(component.key()).properties)if(field.reference=="node"){
            const auto reference=component.value().at("data").at(field.name).get<std::string>();if(!reference.empty()&&!included.count(reference))throw std::invalid_argument("Prefab component depends on an external object");}
        resources.insert(node.at("resourceId").get<std::string>());document["nodes"].push_back(std::move(node));
    }
    for(const auto& resource:expanded.at("resources"))if(resources.count(resource.at("id").get<std::string>()))document["resources"].push_back(resource);
    for(const auto& light:expanded.value("lights",Json::array()))if(included.count(light.at("nodeId").get<std::string>()))document["lights"].push_back(light);
    auto candidate=Level::parse(document,context.assets());for(const auto& node:candidate.scene.nodes)context.validateComponentReferences(node,candidate.components.at(node.id),candidate.scene);
    session.contentDirectories().cancelRefresh();auto& content=session.content();const auto plan=content.plan({{"operation","prefab.write"},{"destination",path},{"document",document},{"create",true}});auto assets=context.assets();const auto result=content.apply(plan.at("id"));
    try{assets.refresh();context.assets()=std::move(assets);}catch(...){content.undo();throw;}
    return {{"asset",context.assets().idForPath(path)},{"transaction",result},{"nodes",included.size()}};
}
Json PrefabAuthoringService::overrides(EditorContext& context,const std::string& id) const{
    const auto row=instanceRow(context,id);const auto source=read(context.assets().resolveReference(row.at("asset")));const auto fingerprint=context.prefabFingerprints_.at(id);
    const auto fields=fieldsFor(context.prefabBaselines_.at(id),localNodes(context,id));return {{"instance",id},{"asset",row.at("asset")},{"baseFingerprint",fingerprint},{"sourceChanged",fingerprint!=documentFingerprint(source)},{"fields",fields}};
}
void PrefabAuthoringService::revert(EditorContext& context,const std::string& id,const Json& fields){
    const auto description=overrides(context,id);if(description.at("sourceChanged").get<bool>())throw std::invalid_argument("Reload the source before reverting expired fields");
    auto current=localNodes(context,id);copyFields(current,context.prefabBaselines_.at(id),fields,description.at("fields"));auto document=context.levelDocument();
    for(auto& instance:document["prefabs"])if(instance.at("instance")==id){instance["overrides"]=Json::object();for(const auto& node:current.items()){auto base=context.prefabBaselines_.at(id).at(node.key()),value=node.value();base.erase("id");value.erase("id");const auto patch=prefabOverride(base,value);if(!patch.empty())instance["overrides"][node.key()]=patch;}}
    adopt(context,document);
}
Json PrefabAuthoringService::apply(EditorSession& session,const std::string& id,const Json& fields,const std::string& fingerprint){
    auto& context=session.context();const auto description=overrides(context,id);
    if(description.at("sourceChanged").get<bool>()||description.at("baseFingerprint")!=fingerprint)throw std::invalid_argument("Prefab source fingerprint is expired");
    const auto reference=description.at("asset").get<std::string>();const auto asset=context.assets().records().count(reference)?reference:context.assets().idForPath(reference);const auto& record=context.assets().records().at(asset);auto before=read(record.path),after=before;
    if(after.contains("prefabs")&&!after["prefabs"].empty())throw std::invalid_argument("Apply fields to a direct prefab source");
    auto sourceNodes=nodes(before);copyFields(sourceNodes,localNodes(context,id),fields,description.at("fields"));for(auto& node:after["nodes"])node=sourceNodes.at(node.at("id").get<std::string>());
    const auto sourceReference=instanceRow(context,id).at("asset").get<std::string>();std::map<std::string,Json> replacements{{sourceReference,after},{asset,after}};
    std::vector<EditorContext*> affected;auto ids=Json::array();Impl::History history;history.asset=asset;history.path=record.virtualPath;history.sourceBefore=before;history.sourceAfter=after;
    for(const auto& row:session.documents().describe(context.documentId())){
        auto& document=*session.documents().at(row.at("id")).context;bool used=false;
        for(const auto& instance:document.sourceLevel_.value("prefabs",Json::array()))if(document.assets().resolveReference(instance.at("asset"))==record.path){
            if(document.prefabFingerprints_.at(instance.at("instance").get<std::string>())!=fingerprint)throw std::invalid_argument("An open document owns an expired prefab source");used=true;replacements[instance.at("asset").get<std::string>()]=after;}
        if(used){affected.push_back(&document);ids.push_back(row.at("id"));history.documents.push_back({row.at("id"),document.snapshot(),{}, {}});}
    }
    DocumentBatchTransaction transaction(affected);for(auto* document:affected)adopt(transaction.candidate(*document),document->levelDocument(),replacements);
    session.contentDirectories().cancelRefresh();auto& content=session.content();const auto plan=content.plan({{"operation","prefab.write"},{"destination",record.virtualPath},{"document",after},{"create",false}});
    bool published=false;try{transaction.commit([&]{content.apply(plan.at("id"));published=true;for(auto* document:affected)document->assets().refresh();},[&]{if(published)content.undo();});}catch(...){content.discard(plan.at("id"));throw;}
    for(auto& document:history.documents){const auto& target=*session.documents().at(document.id).context;document.after=target.documentVersion();document.applied=target.snapshot();}if(impl_->history.size()==8)impl_->history.erase(impl_->history.begin());impl_->history.push_back(std::move(history));impl_->redo.clear();
    return {{"asset",asset},{"documents",ids},{"fields",fields},{"baseFingerprint",documentFingerprint(after)}};
}
void PrefabAuthoringService::undoApply(EditorSession& session){restoreApplication(session,false);}
void PrefabAuthoringService::redoApply(EditorSession& session){restoreApplication(session,true);}
void PrefabAuthoringService::restoreApplication(EditorSession& session,bool forward){
    auto& from=forward?impl_->redo:impl_->history;auto& destination=forward?impl_->history:impl_->redo;
    if(from.empty())throw std::invalid_argument("Prefab source application history is empty");auto history=from.back();auto& context=session.context();
    if(read(context.assets().resolve(history.asset))!=(forward?history.sourceBefore:history.sourceAfter))throw std::invalid_argument("Prefab source changed after application");
    std::vector<EditorContext*> affected;for(const auto& document:history.documents){auto& target=*session.documents().at(document.id).context;if(target.documentVersion()!=document.after)throw std::invalid_argument("An affected document changed after source application");affected.push_back(&target);}
    DocumentBatchTransaction transaction(affected);for(const auto& document:history.documents)transaction.candidate(*session.documents().at(document.id).context).restore(forward?document.applied:document.before);
    session.contentDirectories().cancelRefresh();auto& content=session.content();const auto plan=content.plan({{"operation","prefab.write"},{"destination",history.path},{"document",forward?history.sourceAfter:history.sourceBefore},{"create",false}});
    bool published=false;try{transaction.commit([&]{content.apply(plan.at("id"));published=true;for(auto* document:affected)document->assets().refresh();},[&]{if(published)content.undo();});}catch(...){content.discard(plan.at("id"));throw;}
    for(auto& document:history.documents)document.after=session.documents().at(document.id).context->documentVersion();
    from.pop_back();destination.push_back(std::move(history));
    if(!from.empty())for(auto& document:from.back().documents){const auto current=session.documents().at(document.id).context->documentVersion();if(current.contentHash==document.after.contentHash)document.after=current;}
}
}
