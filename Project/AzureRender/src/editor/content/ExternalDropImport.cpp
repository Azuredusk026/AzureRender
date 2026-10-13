#include "editor/EditorSession.hpp"
#include "editor/AssetImportJob.hpp"
#include "editor/commands/EditTransaction.hpp"
#include "editor/content/SceneNaming.hpp"
#include "runtime/AssetTypeRegistry.hpp"
namespace azurerender {
bool EditorSession::dropImporting() const {
    for(const auto& task:tasks_->report())if(task.at("source")=="asset.import-drop"&&task.at("state")=="running")return true;
    return false;
}
std::string EditorSession::startDropImport(const nlohmann::json& request) {
    if(!context_->isProject()||context_->importing()||dropImporting()||reimporting_||playing()||building())throw EditRejection("File drop requires an idle project");
    const auto captured=context_;const auto version=captured->documentVersion();
    const auto parent=request.value("parent",std::string());
    if(!parent.empty()&&std::none_of(captured->scene().nodes.begin(),captured->scene().nodes.end(),[&](const auto& node){return node.id==parent;}))throw EditRejection("Unknown drop parent");
    const auto position=request.value("position",std::array<float,3>{0,0,0});
    for(const auto coordinate:position)if(!std::isfinite(coordinate))throw EditRejection("Drop position must be finite");
    const auto source=std::filesystem::u8path(request.at("path").get<std::string>());
    const auto identity=newDocumentIdentity();const auto base=captured->project().file.parent_path();
    auto job=std::make_shared<AssetImportJob>(source,base/".azure/imports"/identity,captured->project().mounts.begin()->second/"imports"/identity);
    contentDirectories_.cancelRefresh();
    return tasks_->add("asset.import-drop",source.u8string(),{
        [job]{return job->progress();},[job]{return job->ready();},
        [this,captured,version,job,request,parent,position,source]{
            if(context_!=captured||captured->documentVersion()!=version)throw EditRejection("Drop document changed; retry the file drop");
            try{
                EditTransaction transaction(*captured);auto& candidate=transaction.candidate();
                const auto asset=candidate.publishPreparedImport(*job);
                const auto* type=assetTypeRegistry().classify(candidate.assets().resolveReference(asset));
                const bool place=request.value("place",false)&&type&&type->placeable;
                if(place){candidate.placeAssetAt(asset,position);
                    if(type->id=="model"){candidate.addGameplayComponent("azure.rigid-body");candidate.setComponentFields("azure.rigid-body",{{"shape","mesh"},{"mesh",asset}});}
                    if(!parent.empty())candidate.reparentNode(candidate.selectedNode()->id,parent);
                    candidate.setSelectedNodeName(nextObjectName(candidate.scene(),source.stem().u8string()));}
                transaction.validate();transaction.commit({});assetReloadRequested_=true;
                if(place)selection_->requestReveal();
                return nlohmann::json{{"asset",asset},{"placed",place},{"summary",captured->importSummary()}};
            }catch(...){std::filesystem::remove_all(job->destination);throw;}
        },[job]{job->cancel();}});
}
}
