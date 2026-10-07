#include "editor/EditorContext.hpp"
#include "editor/AssetImportJob.hpp"
#include "runtime/Prefab.hpp"
#include "runtime/LevelRenderSettings.hpp"
#include "resources/ResourceLocator.hpp"
#include "assets/GltfLoader.hpp"
#include "runtime/ComponentCodec.hpp"
#include "runtime/AnimationStateMachine.hpp"
#include "runtime/AssetTypeRegistry.hpp"
#include <chrono>
#include <fstream>
#include <set>
namespace azurerender {
namespace {
std::string identity(const char* prefix){static std::uint64_t count=0;return std::string(prefix)+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+"-"+std::to_string(++count);}
}
std::shared_ptr<EditorContext> EditorContext::openProject(const std::filesystem::path& path){
    runtimeComponentRegistry().seal();
    auto project=std::make_unique<Project>(Project::load(path));auto assets=std::make_unique<AssetDatabase>(*project);assets->refresh();
    const auto scenePath=project->resolve(project->startupScene);
    Level level; if(scenePath.extension()==".azurelevel")level=Level::load(scenePath,*assets);else level.scene=project->loadStartupScene();
    auto context=std::make_shared<EditorContext>(level.scene,scenePath);
    context->project_=std::move(project);context->assets_=std::move(assets);context->components_=std::move(level.components);
    if(scenePath.extension()==".azurelevel"){
        std::ifstream file(scenePath);file>>context->sourceLevel_;
        const auto expanded=expandPrefabs(context->sourceLevel_,*context->assets_);
        for(const auto& resource:expanded.at("resources"))context->resourceReferences_[context->assets_->resolveReference(resource.at("asset").get<std::string>())]=resource.at("asset").get<std::string>();
    }else for(const auto& resource:context->scene_.resources){
        for(const auto& mount:context->project_->mounts){auto relative=resource.path.lexically_relative(mount.second);if(!relative.empty() && *relative.begin()!="..")context->resourceReferences_[resource.path]=mount.first+":/"+relative.generic_string();}
        if(!context->resourceReferences_.count(resource.path)){const auto relative=resource.path.lexically_relative(ResourceLocator().publicAsset(""));if(relative.empty()||*relative.begin()=="..")throw std::runtime_error("Resource has no portable project identity");context->resourceReferences_[resource.path]="engine:/assets_public/"+relative.generic_string();}
    }
    context->syncComponents();context->checkpointSaved();return context;
}
std::string EditorContext::commitImport(AssetImportJob& job){
    const auto path=job.finish();
    try{auto candidate=std::make_unique<AssetDatabase>(*assets_);candidate->refresh();const auto mount=project_->mounts.begin();const auto reference=mount->first+":/"+path.lexically_relative(mount->second).generic_string();
        const auto id=candidate->idForPath(reference);const auto sidecar=path.string()+".azmeta";
        std::ifstream input(sidecar);nlohmann::json metadata;input>>metadata;input.close();metadata["importGeneration"]=job.summary().at("generation");
        std::ofstream output(sidecar);output<<metadata.dump(2);output.close();if(!output)throw std::runtime_error("Cannot save import provenance");candidate->refresh(true);
        beginEdit();assets_.swap(candidate);scene_.resources.push_back({id,"gltf",path});resourceReferences_[path]=id;
        importSummary_=job.summary();log("Imported asset: "+path.filename().string()+" ("+std::to_string(importSummary_.at("vertices").get<std::size_t>())+" vertices)");return id;
    }catch(...){std::filesystem::remove_all(job.destination);throw;}
}
std::string EditorContext::importAsset(const std::filesystem::path& path){
    if(!assets_)throw std::logic_error("Asset import requires a project");const auto id=identity("asset-");
    AssetImportJob job(path,project_->file.parent_path()/".azure/imports"/id,project_->mounts.begin()->second/"imports"/id);return commitImport(job);
}
void EditorContext::startImport(const std::filesystem::path& path){
    if(!assets_||importJob_)throw std::logic_error("Import requires an idle project");const auto id=identity("asset-");
    importJob_=std::make_shared<AssetImportJob>(path,project_->file.parent_path()/".azure/imports"/id,project_->mounts.begin()->second/"imports"/id);
}
void EditorContext::cancelImport(){if(importJob_)importJob_->cancel();}
float EditorContext::importProgress() const{return importJob_?importJob_->progress():0;}
bool EditorContext::importReady() const{return !importJob_||importJob_->ready();}
std::optional<std::string> EditorContext::pollImport(){if(!importJob_||!importJob_->ready())return {};auto job=std::move(importJob_);return commitImport(*job);}
void EditorContext::placeResource(const std::string& resource,const std::string& nodeId){
    const auto id=nodeId.empty()?identity("node-"):nodeId;
    if(id.size()>128 || std::any_of(scene_.nodes.begin(),scene_.nodes.end(),[&](const auto& node){return node.id==id;}))throw std::invalid_argument("Duplicate or invalid node identity: "+id);
    auto reference=resource;std::filesystem::path attach;
    if(std::none_of(scene_.resources.begin(),scene_.resources.end(),[&](const auto& entry){return entry.id==resource;})){
        if(!assets_)throw std::invalid_argument("Unknown resource");
        const auto path=assets_->resolveReference(resource);
        if(path.extension()!=".gltf"&&path.extension()!=".glb")throw std::invalid_argument("Placement requires a registered model asset");
        const auto existing=std::find_if(scene_.resources.begin(),scene_.resources.end(),[&](const auto& entry){return resolvedResourcePath(entry).lexically_normal()==path.lexically_normal();});
        if(existing!=scene_.resources.end())reference=existing->id;
        else{
            static_cast<void>(loadGltfAsset(path.string()));
            const auto record=std::find_if(assets_->records().begin(),assets_->records().end(),[&](const auto& entry){return entry.second.path==path;});
            if(record==assets_->records().end())throw std::invalid_argument("Model requires an asset database identity");
            reference=record->first;attach=path;
        }
    }
    beginEdit();
    if(!attach.empty()){scene_.resources.push_back({reference,"gltf",attach});resourceReferences_[attach]=reference;}
    SceneNode node;node.id=id;node.name="Placed Object";node.resourceId=reference;scene_.nodes.push_back(std::move(node));rebuildEntities();selectNode(scene_.nodes.size()-1);
}
void EditorContext::createNode(const std::string& requestedId){
    const auto nodeId=requestedId.empty()?identity("node-"):requestedId;
    if(nodeId.empty() || nodeId.size()>128 || std::any_of(scene_.nodes.begin(),scene_.nodes.end(),[&](const auto& node){return node.id==nodeId;}))throw std::invalid_argument("Duplicate or invalid node identity: "+nodeId);
    beginEdit();SceneNode node;node.id=nodeId;node.name=nodeId;node.visible=false;scene_.nodes.push_back(std::move(node));rebuildEntities();selectNode(scene_.nodes.size()-1);
}
void EditorContext::placePrefab(const std::string& asset,const std::string& instance){
    if(!assets_)throw std::logic_error("Prefab placement requires a project");
    if(assets_->resolveReference(asset).extension()!=".azureprefab")throw std::invalid_argument("Prefab placement expects .azureprefab");
    auto document=levelDocument();if(!document.contains("schemaVersion"))document["schemaVersion"]=1;
    if(!document.contains("prefabs"))document["prefabs"]=nlohmann::json::array();
    document["prefabs"].push_back({{"instance",instance},{"asset",asset}});
    auto candidate=Level::parse(document,*assets_);
    for(const auto& node:candidate.scene.nodes)if(node.id.rfind(instance+":",0)==0)
        validateComponentReferences(node,candidate.components.at(node.id),candidate.scene);
    auto references=resourceReferences_;
    const auto expanded=expandPrefabs(document,*assets_);
    for(const auto& resource:expanded.at("resources")){
        const auto reference=resource.at("asset").get<std::string>();references[assets_->resolveReference(reference)]=reference;
    }
    beginEdit();sourceLevel_=std::move(document);resourceReferences_=std::move(references);
    scene_=std::move(candidate.scene);components_=std::move(candidate.components);rebuildEntities();
    for(std::size_t i=0;i<scene_.nodes.size();++i)if(scene_.nodes[i].id.rfind(instance+":",0)==0){selectNode(i);break;}
    log("Placed prefab: "+instance);
}
void EditorContext::previewAnimation(const std::string& state,double time,const std::string& previous,double crossfade){
    if(!selectedNode() || !assets_)throw std::logic_error("Animation preview requires a project node");
    const auto data=runtimeComponents().at(selectedNode()->id);
    if(!data.contains("azure.animator"))throw std::invalid_argument("Selected node lacks animator");
    auto candidate=data;candidate["azure.animator"]["data"]["state"]=state;
    validateComponentReferences(*selectedNode(),candidate,scene_);
    const auto fields=candidate.at("azure.animator").at("data");
    std::ifstream input(assets_->resolveReference(fields.at("asset").get<std::string>()));nlohmann::json graph;input>>graph;
    auto machine=AnimationStateMachine::parse(graph);
    if(!previous.empty())machine.select(previous);
    machine.select(state,previous.empty()?0:crossfade);machine.advance(time);
    NodeAnimationFrame frame{selectedNode()->id,machine.clip(),machine.time(),machine.loop(),
        machine.previousClip(),machine.previousTime(),machine.blend(),machine.previousLoop(),
        {fields.value("morph0",0.0F),fields.value("morph1",0.0F)}};
    animationPreview_=std::move(frame);
}
void EditorContext::selectNodes(std::vector<std::size_t> indices){
    for(auto index:indices)if(index>=scene_.nodes.size())throw std::out_of_range("Editor node selection is out of range");
    std::set<std::size_t> seen;indices.erase(std::remove_if(indices.begin(),indices.end(),[&](auto index){return !seen.insert(index).second;}),indices.end());
    if(selectedNodes_!=indices) { ++revision_;closeEditMerge();clearAnimationPreview(); }
    selectedNodes_=std::move(indices);if(!selectedNodes_.empty())selectedNodeIndex_=selectedNodes_.back();refreshSelectedTransform();
}
void EditorContext::duplicateSelection(){
    if(selectedNodes_.empty())return;
    std::set<std::string> selected;for(auto index:selectedNodes_)selected.insert(scene_.nodes.at(index).id);
    bool grew=true;while(grew){grew=false;for(const auto& node:scene_.nodes)if(selected.count(node.parentId)&&selected.insert(node.id).second)grew=true;}
    std::map<std::string,std::string> ids,resources;std::vector<std::size_t> created,source;
    for(std::size_t i=0;i<scene_.nodes.size();++i)if(selected.count(scene_.nodes[i].id)){source.push_back(i);ids[scene_.nodes[i].id]=identity("node-");}
    const auto originalLights=scene_.lights;beginEdit();
    for(auto index:source){auto node=scene_.nodes.at(index);const auto original=node.id;node.id=ids.at(original);node.name+=" Copy";
        if(ids.count(node.parentId))node.parentId=ids.at(node.parentId);
        node.prefabSource.clear();node.instanceOf.clear();components_[node.id]=components_.count(original)?components_.at(original):nlohmann::json::object();
        for(auto& component:components_[node.id].items())for(const auto& field:runtimeComponentRegistry().metadata().type(component.key()).properties)
            if(field.reference=="node"&&component.value().at("data").contains(field.name)){
                auto& reference=component.value()["data"][field.name];const auto found=ids.find(reference.get<std::string>());if(found!=ids.end())reference=found->second;
            }
        if(node.resourceId.find(':')!=std::string::npos){const auto found=std::find_if(scene_.resources.begin(),scene_.resources.end(),[&](const auto& entry){return entry.id==node.resourceId;});
            if(found!=scene_.resources.end()){
                const auto cached=resources.find(node.resourceId);
                if(cached!=resources.end())node.resourceId=cached->second;
                else{auto resource=*found;resource.id=identity("resource-");resources[node.resourceId]=resource.id;node.resourceId=resource.id;scene_.resources.push_back(resource);}
            }}
        created.push_back(scene_.nodes.size());scene_.nodes.push_back(std::move(node));
    }
    for(auto light:originalLights)if(ids.count(light.nodeId)){light.id=identity("light-");light.nodeId=ids.at(light.nodeId);scene_.lights.push_back(std::move(light));}
    rebuildEntities();selectNodes(std::move(created));
}
void EditorContext::deleteSelection(){
    if(selectedNodes_.empty())return;beginEdit();std::set<std::string> removed;
    for(auto index:selectedNodes_){const auto& node=scene_.nodes.at(index);removed.insert(node.id);
        if(!node.prefabSource.empty()){const auto prefix=node.id.substr(0,node.id.find(':'))+":";for(const auto& candidate:scene_.nodes)if(candidate.id.rfind(prefix,0)==0)removed.insert(candidate.id);}}
    bool grew=true;while(grew){grew=false;for(const auto& node:scene_.nodes)if(removed.count(node.parentId)&&removed.insert(node.id).second)grew=true;}
    scene_.nodes.erase(std::remove_if(scene_.nodes.begin(),scene_.nodes.end(),[&](const auto& node){return removed.count(node.id)!=0;}),scene_.nodes.end());
    scene_.lights.erase(std::remove_if(scene_.lights.begin(),scene_.lights.end(),[&](const auto& light){return removed.count(light.nodeId)!=0;}),scene_.lights.end());
    for(const auto& id:removed)components_.erase(id);
    selectedNodeIndex_=0;selectedNodes_=scene_.nodes.empty()?std::vector<std::size_t>{}:std::vector<std::size_t>{0};rebuildEntities();refreshSelectedTransform();
}
std::map<std::string,nlohmann::json> EditorContext::runtimeComponents() const {
    auto result=components_;const auto registry=reflection::makeRuntimeRegistry();
    for(const auto& node:scene_.nodes){auto& data=result[node.id];if(data.is_null())data=nlohmann::json::object();
        ecs::TransformComponent transform{node.translation,node.rotation,node.scale};ecs::RenderableComponent renderable{0,node.visible};
        data["azure.transform"]=registry.encode("azure.transform",&transform);data["azure.renderable"]=registry.encode("azure.renderable",&renderable);}
    return result;
}
nlohmann::json EditorContext::componentData(const std::string& node,const std::string& type)const{
    if(type=="azure.transform" || type=="azure.renderable"){
        const auto found=std::find_if(scene_.nodes.begin(),scene_.nodes.end(),[&](const auto& value){return value.id==node;});
        if(found==scene_.nodes.end())return {};
        static const auto registry=reflection::makeRuntimeRegistry();
        if(type=="azure.transform"){ecs::TransformComponent value{found->translation,found->rotation,found->scale};return registry.encode(type,&value).at("data");}
        ecs::RenderableComponent value{0,found->visible};return registry.encode(type,&value).at("data");
    }
    const auto found=components_.find(node);
    if(found==components_.end() || !found->second.contains(type))return {};
    auto fields=runtimeComponentRegistry().defaults(type).at("data");fields.update(found->second.at(type).at("data"));return fields;
}
void EditorContext::validateComponentReferences(const SceneNode& node,const nlohmann::json& data,const SceneDocument& scene)const {
    validateComponents(data);
    if(data.contains("azure.character") && data.contains("azure.rigid-body"))
        throw std::invalid_argument("Node "+node.id+": character and rigid-body require separate nodes");
    for(const auto& component:data.items())for(const auto& field:runtimeComponentRegistry().metadata().type(component.key()).properties){
        if(field.reference.empty()||!component.value().at("data").contains(field.name))continue;
        const auto reference=component.value().at("data").at(field.name).get<std::string>();if(reference.empty())continue;
        if(field.reference=="node"){
            if(std::none_of(scene.nodes.begin(),scene.nodes.end(),[&](const auto& value){return value.id==reference;}))throw std::invalid_argument("Node "+node.id+": unknown node reference "+field.name+" = "+reference);
        }else{
            if(!assets_)throw std::logic_error("Asset references require a project");const auto path=assets_->resolveReference(reference);
            if(!std::filesystem::is_regular_file(path)||!assetTypeRegistry().matches(path,field.assetTypes))throw std::invalid_argument("Node "+node.id+": incompatible or missing asset "+field.name+" = "+reference);
        }
    }
    if(data.contains("azure.animator")){
        const auto fields=data.at("azure.animator").at("data");const auto reference=fields.value("asset",std::string());
        if(!reference.empty()){
            std::ifstream input(assets_->resolveReference(reference));nlohmann::json graph;input>>graph;
            auto machine=AnimationStateMachine::parse(graph);machine.select(fields.value("state",std::string("idle")));
            const auto resource=std::find_if(scene.resources.begin(),scene.resources.end(),[&](const auto& r){return r.id==node.resourceId;});
            if(resource==scene.resources.end())throw std::invalid_argument("Node "+node.id+": animator requires a model");
            const auto model=loadGltfAsset(resolvedResourcePath(*resource).u8string());
            for(const auto& state:graph.at("states"))if(state.at("clip").get<std::size_t>()>=model.animations.size())
                throw std::invalid_argument("Node "+node.id+": animation clip exceeds model clips: "+state.at("name").get<std::string>());
        }
    }
    if(data.contains("azure.third-person-camera")){
        game::ThirdPersonCamera camera;reflection::makeRuntimeRegistry().decode("azure.third-person-camera",&camera,data.at("azure.third-person-camera"));
        if(std::none_of(scene.nodes.begin(),scene.nodes.end(),[&](const auto& candidate){return candidate.id==camera.target;}))
            throw std::invalid_argument("Node "+node.id+": unknown camera target "+camera.target);
        if(camera.minimumDistance>camera.distance || camera.distance>camera.maximumDistance || camera.minimumPitch>camera.maximumPitch)
            throw std::invalid_argument("Node "+node.id+": inconsistent camera limits");
    }
}
void EditorContext::addGameplayComponent(const std::string& type){
    if(!selectedNode())return;
    if(!componentData(selectedNode()->id,type).is_null())return;
        auto data=runtimeComponents().at(selectedNode()->id);data[type]=runtimeComponentRegistry().defaults(type);
        for(const auto& field:runtimeComponentRegistry().metadata().type(type).properties)
            if(field.referenceDefault=="selected-node")data[type]["data"][field.name]=selectedNode()->id;
        validateComponentReferences(*selectedNode(),data,scene_);beginEdit();components_[selectedNode()->id]=std::move(data);syncComponents();
}
void EditorContext::setComponentField(const std::string& type,const std::string& field,const nlohmann::json& value){
    if(!selectedNode())throw std::logic_error("Component editing requires selection");
    auto data=runtimeComponents().at(selectedNode()->id);if(!data.contains(type))throw std::invalid_argument("Node lacks component");
    runtimeComponentRegistry().validateWrite(type,field,value);
    data[type]["data"][field]=value;validateComponentReferences(*selectedNode(),data,scene_);
    beginEdit();components_[selectedNode()->id]=data;
    if(type=="azure.transform"){ecs::TransformComponent transform;reflection::makeRuntimeRegistry().decode(type,&transform,data[type]);auto* node=selectedNode();node->translation=transform.translation;node->rotation=transform.rotation;node->scale=transform.scale;refreshSelectedTransform();}
    if(type=="azure.renderable")selectedNode()->visible=data[type]["data"].at("visible").get<bool>();
    syncComponents();
}
nlohmann::json EditorContext::levelDocument()const{
    if(!assets_)throw std::logic_error("Level serialization requires a project");
    auto document=sourceLevel_;document["id"]=scene_.sceneId;document["nodes"]=nlohmann::json::array();document["resources"]=nlohmann::json::array();document["lights"]=nlohmann::json::array();
    document["renderSettings"]=encodeLevelRenderSettings(renderSettings());
    const auto data=runtimeComponents();std::set<std::string> prefabResources,liveInstances;
    for(const auto& node:scene_.nodes){
        nlohmann::json encoded={{"id",node.id},{"name",node.name},{"parentId",node.parentId},{"resourceId",node.resourceId},{"components",data.at(node.id)}};
        if(!node.prefabSource.empty()&&!node.instanceOf.empty()){
            const auto prefix=node.id.substr(0,node.id.find(':'));liveInstances.insert(prefix);prefabResources.insert(node.resourceId);
            bool found=false;for(auto& instance:document["prefabs"])if(instance.at("instance")==prefix){
                auto local=encoded;local.erase("id");for(const char* link:{"parentId","resourceId"}){const auto text=local[link].get<std::string>();if(!text.empty())local[link]=text.substr(prefix.size()+1);}
                instance["overrides"][node.instanceOf].merge_patch(local);found=true;break;}
            if(!found)throw std::runtime_error("Prefab node lacks serialized instance");
        }else document["nodes"].push_back(std::move(encoded));
    }
    if(document.contains("prefabs")){auto& instances=document["prefabs"];instances.erase(std::remove_if(instances.begin(),instances.end(),[&](const auto& instance){return !liveInstances.count(instance.at("instance").template get<std::string>());}),instances.end());}
    for(const auto& resource:scene_.resources){if(prefabResources.count(resource.id))continue;
        if(resource.id.find(':')!=std::string::npos && std::none_of(scene_.nodes.begin(),scene_.nodes.end(),[&](const auto& node){return node.resourceId==resource.id;}))continue;
        std::string reference=resourceReferences_.count(resource.path)?resourceReferences_.at(resource.path):std::string();
        for(const auto& original:sourceLevel_.value("resources",nlohmann::json::array()))if(original.at("id")==resource.id)reference=original.at("asset").get<std::string>();
        if(reference.empty())for(const auto& record:assets_->records())if(record.second.path==resource.path){reference=record.first;break;}
        if(reference.empty())throw std::runtime_error("Resource lacks project asset identity: "+resource.id);
        document["resources"].push_back({{"id",resource.id},{"asset",reference}});
    }
    for(const auto& light:scene_.lights){if(light.nodeId.find(':')!=std::string::npos && liveInstances.count(light.nodeId.substr(0,light.nodeId.find(':'))))continue;
        document["lights"].push_back({{"id",light.id},{"nodeId",light.nodeId},{"color",light.color},{"intensity",light.intensity},{"radius",light.radius},{"enabled",light.enabled}});}
    return document;
}
}
