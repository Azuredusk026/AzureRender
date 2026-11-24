#include "SceneImportService.hpp"
#include "editor/EditorSession.hpp"
#include "editor/commands/EditTransaction.hpp"
#include "runtime/ComponentRegistry.hpp"
#include "assets/GltfLoader.hpp"
#include "assets/AssetTransfer.hpp"
#include <algorithm>
#include <atomic>
#include <future>
#include <set>
namespace azurerender {
namespace {
using Json=nlohmann::json;
std::string portable(const EditorContext& context,const std::filesystem::path& path){
 const auto resolved=std::filesystem::weakly_canonical(path);
 for(const auto& [name,mount]:context.project().mounts){const auto relative=resolved.lexically_relative(std::filesystem::weakly_canonical(mount));if(!relative.empty()&&*relative.begin()!="..")return name+":/"+relative.generic_u8string();}
 throw EditRejection("Place the complete scene exchange package inside a project asset mount before import");
}
Json nodeTable(const Json& document){auto table=Json::object();for(const auto& node:document.at("nodes"))table[node.at("id").get<std::string>()]=node;return table;}
}
Json SceneImportService::apply(EditorContext& context,const SceneImportProposal& proposal){
 if(!context.isProject()||context.scenePath().extension()!=".azurelevel")throw EditRejection("Scene exchange requires a project level document");
 for(const auto& [path,fingerprint]:proposal.sourceFingerprints)if(assetTransferFingerprint(path)!=fingerprint)throw EditRejection("Scene source changed while importing; retry");
 const auto reference=portable(context,proposal.source);auto existing=context.levelDocument();auto imports=existing.value("sceneImports",Json::object());
 const auto key=proposal.sourceId;const auto old=imports.value(key,Json::object());auto mapping=old.value("nodeMap",Json::object());auto resourceMap=old.value("resourceMap",Json::object());
 if(!old.empty()&&old.at("source")!=reference)throw EditRejection("Source ID belongs to another exchange package");
 const auto current=nodeTable(existing);std::set<std::string> oldNodes,oldLights;
 if(!old.empty()){
  for(const auto& [id,baseline]:old.at("baseline").items()){oldNodes.insert(id);if(!current.contains(id)||current.at(id)!=baseline)throw EditRejection("Scene reimport conflict: edited or deleted node "+id);}
  for(const auto& node:context.scene_.nodes)if(!oldNodes.count(node.id)&&oldNodes.count(node.parentId))throw EditRejection("Scene reimport conflict: locally attached child");
  for(const auto& baseline:old.at("lights")){const auto id=baseline.at("id").get<std::string>();oldLights.insert(id);auto found=std::find_if(existing.at("lights").begin(),existing.at("lights").end(),[&](const auto& value){return value.at("id")==id;});if(found==existing.at("lights").end()||*found!=baseline)throw EditRejection("Scene reimport conflict: edited light");}
 }
 // Asset identities are already registered by the project content service.
 // This transaction writes document state only; rejected proposals cannot publish files.
 for(const auto& resource:proposal.scene.resources){
  const auto asset=context.assets().idForPath(portable(context,resource.path));if(asset.empty())throw EditRejection("Refresh project assets before scene import");
  if(!resourceMap.contains(resource.id))resourceMap[resource.id]="exchange-resource-"+newDocumentIdentity();
  const auto id=resourceMap.at(resource.id).get<std::string>();auto found=std::find_if(context.scene_.resources.begin(),context.scene_.resources.end(),[&](const auto& item){return item.id==id;});
  if(found==context.scene_.resources.end())context.scene_.resources.push_back({id,"gltf",resource.path});else if(found->path!=resource.path)throw EditRejection("Scene reimport conflict: resource reference changed");
  context.resourceReferences_[resource.path]=asset;
 }
 for(const auto& node:proposal.scene.nodes)if(!mapping.contains(node.id))mapping[node.id]="exchange-node-"+newDocumentIdentity();
 context.scene_.nodes.erase(std::remove_if(context.scene_.nodes.begin(),context.scene_.nodes.end(),[&](const auto& node){return oldNodes.count(node.id)!=0;}),context.scene_.nodes.end());
 context.scene_.lights.erase(std::remove_if(context.scene_.lights.begin(),context.scene_.lights.end(),[&](const auto& light){return oldLights.count(light.id)!=0;}),context.scene_.lights.end());
 for(const auto& id:oldNodes)context.components_.erase(id);
 auto activeMap=Json::object();
 for(auto node:proposal.scene.nodes){const auto sourceId=node.id;node.id=mapping.at(sourceId).get<std::string>();activeMap[sourceId]=node.id;
  if(!node.parentId.empty())node.parentId=mapping.at(node.parentId).get<std::string>();if(!node.resourceId.empty())node.resourceId=resourceMap.at(node.resourceId).get<std::string>();
  if(const auto components=proposal.components.find(sourceId);components!=proposal.components.end())for(const auto& [type,value]:components->second.items()){
   auto fields=runtimeComponentRegistry().defaults(type);fields["data"].update(value.at("data"));context.components_[node.id][type]=std::move(fields);
  }
  context.scene_.nodes.push_back(std::move(node));
 }
 auto lightMap=old.value("lightMap",Json::object());std::set<std::string> importedLights;
 for(auto light:proposal.scene.lights){if(!lightMap.contains(light.id))lightMap[light.id]="exchange-light-"+newDocumentIdentity();light.id=lightMap.at(light.id).get<std::string>();light.nodeId=mapping.at(light.nodeId).get<std::string>();importedLights.insert(light.id);context.scene_.lights.push_back(light);}
 context.refreshMaterialBindings();context.rebuildEntities();context.selectNodes({});
 const auto updated=context.levelDocument();auto baseline=Json::object();for(const auto& node:updated.at("nodes"))for(const auto& [sourceId,id]:activeMap.items()){(void)sourceId;if(node.at("id")==id)baseline[id.get<std::string>()]=node;}
 auto lightBaseline=Json::array();for(const auto& light:updated.at("lights"))if(importedLights.count(light.at("id").get<std::string>()))lightBaseline.push_back(light);
 imports[key]={{"source",reference},{"nodeMap",activeMap},{"resourceMap",resourceMap},{"lightMap",lightMap},{"baseline",baseline},{"lights",lightBaseline},{"diagnostics",proposal.diagnostics}};
 context.sourceLevel_["sceneImports"]=std::move(imports);
 return {{"sourceId",key},{"nodeMap",activeMap},{"resources",proposal.scene.resources.size()},{"diagnostics",proposal.diagnostics}};
}
std::string SceneImportService::start(EditorSession& session,const std::filesystem::path& source){
 if(!session.context_->isProject()||session.playing()||session.building())throw EditRejection("Scene import requires an idle project");
 const auto captured=session.context_;const auto version=captured->documentVersion();const auto assetVersion=captured->assets().version();(void)portable(*captured,source);
 auto cancelled=std::make_shared<std::atomic<bool>>(false);
 auto future=std::make_shared<std::future<SceneImportProposal>>(std::async(std::launch::async,[source,cancelled]{
  auto proposal=readSceneExchange(source,[cancelled]{return cancelled->load();});
  for(const auto& resource:proposal.scene.resources){if(cancelled->load())throw EditRejection("Scene import cancelled");static_cast<void>(loadGltfAsset(resource.path.u8string()));}return proposal;
 }));
 return session.tasks_->add("scene.import",source.u8string(),{
  []{return .5F;},[future]{return future->wait_for(std::chrono::seconds(0))==std::future_status::ready;},
  [&session,captured,version,assetVersion,cancelled,future]{auto proposal=future->get();if(cancelled->load())throw EditRejection("Scene import cancelled");if(session.context_!=captured||captured->documentVersion()!=version||captured->assets().version()!=assetVersion)throw EditRejection("Scene document or asset database changed while importing; retry");
   EditTransaction transaction(*captured);auto result=apply(transaction.candidate(),proposal);transaction.validate();transaction.commit({});session.assetReloadRequested_=true;return result;
  },[cancelled]{cancelled->store(true);}});
}
void registerSceneImportOperations(EditRegistry& registry,EditorSession& session){
 const Json schema={{"type","object"},{"properties",{{"path",{{"type","string"}}}}},{"required",{"path"}}};
 registry.add({"scene.import",1,schema,true,true,true},[](EditorContext& context,const Json& args){const auto proposal=readSceneExchange(std::filesystem::u8path(args.at("path").get<std::string>()));for(const auto& resource:proposal.scene.resources)static_cast<void>(loadGltfAsset(resource.path.u8string()));return SceneImportService::apply(context,proposal);});
 registry.add({"scene.import-start",1,schema,false,false,true},[&session](EditorContext&,const Json& args)->Json{return {{"task",SceneImportService::start(session,std::filesystem::u8path(args.at("path").get<std::string>()))}};});
}
}
