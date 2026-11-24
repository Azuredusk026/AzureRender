#include "editor/content/AssetExportService.hpp"
#include "editor/EditorContext.hpp"
#include "assets/AssetExportJob.hpp"
#include "assets/model/ModelExportScene.hpp"
#include "scene/SceneDescription.hpp"
#include <algorithm>
#include <chrono>
#include <set>
namespace azurerender {
nlohmann::json AssetExportService::plan(EditorContext& context,const nlohmann::json& parameters){
    if(!context.isProject()||context.dirty()||context.importing())throw std::invalid_argument("Export planning requires a saved idle project");
    if(parameters.contains("asset")==parameters.contains("node"))throw std::invalid_argument("Export requires exactly one asset or subtree root");
    if(plans_.size()>=64)plans_.erase(plans_.begin());
    AssetTransferRequest request;request.operation=AssetTransferOperation::ExportPlan;request.formatId=parameters.at("format").get<std::string>();
    request.destination=std::filesystem::u8path(parameters.at("destination").get<std::string>()).lexically_normal();
    if(!request.destination.is_absolute())throw std::invalid_argument("Export destination must be an absolute directory");
    request.settings=parameters.value("settings",nlohmann::json::object());request.settingsVersion=parameters.value("settingsVersion",1U);
    const auto identity="export-plan-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+"-"+std::to_string(++sequence_);
    request.staging=context.project().file.parent_path()/".azure/exports"/identity;
    if(parameters.contains("asset"))request.source=context.assets().resolveReference(parameters.at("asset").get<std::string>());
    else{
        const auto id=parameters.at("node").get<std::string>();const auto& document=context.scene();
        if(std::none_of(document.nodes.begin(),document.nodes.end(),[&](const auto& node){return node.id==id;}))throw std::invalid_argument("Unknown export subtree root");
        std::set<std::string> selected{id};bool changed=true;
        while(changed){changed=false;for(const auto& node:document.nodes)if(selected.count(node.parentId)&&selected.insert(node.id).second)changed=true;}
        const auto worldTransforms=scene::resolveNodeWorldTransforms(document.renderDescription());auto scene=std::make_shared<ModelExportScene>();
        for(const auto& node:document.nodes)if(selected.count(node.id)){
            ModelExportNode exported;exported.id=node.id;exported.parent=selected.count(node.parentId)?node.parentId:std::string();exported.transform=node.id==id?worldTransforms.at(static_cast<std::size_t>(&node-document.nodes.data())):scene::composeTrs(node.translation,node.rotation,node.scale);
            if(!node.resourceId.empty()){const auto resource=std::find_if(document.resources.begin(),document.resources.end(),[&](const auto& value){return value.id==node.resourceId;});if(resource==document.resources.end())throw std::invalid_argument("Subtree contains an unresolved model resource");exported.source=resource->path;if(request.source.empty())request.source=exported.source;}
            scene->nodes.push_back(std::move(exported));}
        request.scene=std::move(scene);
    }
    auto report=planAssetExport(request,assetFormatRegistry());request.settings=report.settings;request.operation=AssetTransferOperation::Export;
    auto result=report.summary;result["plan"]=identity;result["source"]=request.source.u8string();result["destination"]=request.destination.u8string();result["format"]=request.formatId;result["dependencies"]=nlohmann::json::array();for(const auto& path:report.dependencies)result["dependencies"].push_back(path.u8string());
    plans_[identity]={request,report,context.documentId(),context.revision(),context.assets().version()};return result;
}
AssetTransferRequest AssetExportService::consume(EditorContext& context,const std::string& identity){
    const auto found=plans_.find(identity);if(found==plans_.end())throw std::invalid_argument("Unknown or consumed export plan");
    auto plan=std::move(found->second);plans_.erase(found);
    if(context.dirty()||context.importing()||context.documentId()!=plan.document||context.revision()!=plan.revision||context.assets().version()!=plan.databaseVersion)throw std::invalid_argument("Export plan document or assets are stale; plan again");
    for(const auto& source:plan.report.sourceFingerprints)if(assetTransferFingerprint(source.first)!=source.second)throw std::runtime_error("Export plan source changed; plan again");
    return plan.request;
}
}
