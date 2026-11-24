#include "editor/commands/EditTransaction.hpp"
#include "runtime/LevelRenderSettings.hpp"
#include "runtime/RuntimeLifecycle.hpp"
#include "runtime/ComponentCodec.hpp"
#include <atomic>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <sstream>
namespace azurerender {
std::string newDocumentIdentity() {
    static std::atomic<std::uint64_t> sequence{0};
    return std::to_string(std::chrono::system_clock::now().time_since_epoch().count())+"-"+std::to_string(++sequence);
}
std::string documentFingerprint(const nlohmann::json& content) {
    std::uint64_t hash=14695981039346656037ULL;
    for(const auto value:content.dump()) { hash^=static_cast<unsigned char>(value);hash*=1099511628211ULL; }
    std::ostringstream text;text<<"fnv1a64:"<<std::hex<<std::setw(16)<<std::setfill('0')<<hash;return text.str();
}
nlohmann::json EditorContext::documentContent() const {
    auto nodes=nlohmann::json::array(),resources=nlohmann::json::array(),lights=nlohmann::json::array();
    for(const auto& node:scene_.nodes)nodes.push_back({{"id",node.id},{"name",node.name},{"parentId",node.parentId},
        {"resourceId",node.resourceId},{"visible",node.visible},{"translation",node.translation},{"rotation",node.rotation},
        {"scale",node.scale},{"prefabSource",node.prefabSource},{"instanceOf",node.instanceOf}});
    for(const auto& resource:scene_.resources)resources.push_back({{"id",resource.id},{"type",resource.type},{"path",resource.path.generic_string()}});
    for(const auto& light:scene_.lights)lights.push_back({{"id",light.id},{"nodeId",light.nodeId},{"color",light.color},
        {"intensity",light.intensity},{"radius",light.radius},{"enabled",light.enabled}});
    const auto& settings=renderSettings();auto render=encodeLevelRenderSettings(settings);
    render["schemaVersion"]=RenderSettings::kSchemaVersion;
    render["sceneType"]=static_cast<unsigned>(settings.sceneType);
    render["renderPath"]=static_cast<unsigned>(settings.renderPath);
    render["styleMaskStrength"]=settings.styleMaskStrength;render["diffuseBandThreshold"]=settings.diffuseBandThreshold;
    render["diagnosticView"]=settings.diagnosticView;render["morphWeights"]=settings.morphWeights;
    render["faceMirror"]=settings.faceSdf.mirrorHorizontal;render["noseShadow"]=settings.faceSdf.noseShadowStrength;
    render["jawShadow"]=settings.faceSdf.jawShadowStrength;render["faceShadowColor"]=settings.faceSdf.shadowColor;
    render["blackholeQuality"]=static_cast<unsigned>(settings.blackhole.quality);
    render["blackholeCamera"]=static_cast<unsigned>(settings.blackhole.camera);
    auto references=nlohmann::json::object();
    for(const auto& reference:resourceReferences_)references[reference.first.generic_string()]=reference.second;
    return {{"id",scene_.sceneId},{"nodes",nodes},{"resources",resources},{"lights",lights},
        {"components",runtimeComponents()},{"sourceLevel",sourceLevel_},{"renderSettings",render},{"resourceReferences",references}};
}
DocumentVersion EditorContext::documentVersion() const { return {documentId_,revision_,documentFingerprint(documentContent())}; }
EditTransaction::EditTransaction(EditorContext& document):document_(document) {
    candidate_=std::make_unique<EditorContext>(document.scene_,document.scenePath_);
    if(document.project_)candidate_->project_=std::make_unique<Project>(*document.project_);
    if(document.assets_)candidate_->assets_=std::make_unique<AssetDatabase>(*document.assets_);
    candidate_->restore(document.snapshot());
    candidate_->dirty_=document.dirty_;candidate_->animationPreview_=document.animationPreview_;
}
void EditTransaction::commit(const std::string& mergeKey) {
    auto& target=document_;
    for(const auto index:candidate_->selectedNodes_)
        if(index>=candidate_->scene_.nodes.size())throw std::invalid_argument("Candidate selection is out of range");
    if(!candidate_->selectedNodes_.empty()&&candidate_->selectedNodeIndex_>=candidate_->scene_.nodes.size())
        throw std::invalid_argument("Candidate active selection is out of range");
    const bool changed=target.documentContent()!=candidate_->documentContent();
    const bool selection=target.selectedNodes_!=candidate_->selectedNodes_;
    if(!changed&&!selection)return;
    if(changed) {
        RuntimeLifecycle validation;validation.loadScene(candidate_->scene_);
        validateRenderSettings(candidate_->renderSettings());
        std::set<std::string> nodeIds,lightIds;
        for(const auto& node:candidate_->scene_.nodes)nodeIds.insert(node.id);
        for(const auto& light:candidate_->scene_.lights) {
            if(light.id.empty()||!lightIds.insert(light.id).second||!nodeIds.count(light.nodeId)
                ||!std::isfinite(light.radius)||light.radius<=0||!std::isfinite(light.intensity)||light.intensity<0)
                throw std::invalid_argument("Invalid candidate light identity, reference or range");
            for(const auto channel:light.color)if(!std::isfinite(channel)||channel<0)
                throw std::invalid_argument("Invalid candidate light color");
        }
        std::set<std::string> resources;
        for(const auto& resource:candidate_->scene_.resources)
            if(resource.id.empty()||resource.type.empty()||resource.path.empty()||!resources.insert(resource.id).second)
                throw std::invalid_argument("Invalid candidate resource identity");
        const auto original=target.runtimeComponents(),components=candidate_->runtimeComponents();
        for(const auto& node:candidate_->scene_.nodes) {
            if(!node.resourceId.empty()&&!resources.count(node.resourceId))throw std::invalid_argument("Candidate node has an unknown resource");
            const auto& data=components.at(node.id);validateComponents(data);
            const auto previous=original.find(node.id);
            if(previous==original.end()||previous->second!=data)
                candidate_->validateComponentReferences(node,data,candidate_->scene_);
        }
        candidate_->rebuildEntities();candidate_->refreshSelectedTransform();
        const bool merge=!mergeKey.empty()&&target.mergeKey_==mergeKey&&target.mergeRevision_==target.revision_
            &&target.mergeSelection_==target.selectedNodes_&&!target.undoStack_.empty();
        if(!merge)target.beginEdit();else ++target.revision_;
        target.scene_=std::move(candidate_->scene_);target.components_=std::move(candidate_->components_);
        target.sourceLevel_=std::move(candidate_->sourceLevel_);target.resourceReferences_=std::move(candidate_->resourceReferences_);
        target.selectedNodeIndex_=candidate_->selectedNodeIndex_;target.selectedNodes_=std::move(candidate_->selectedNodes_);
        target.ecsWorld_.swap(candidate_->ecsWorld_);target.nodeEntities_=std::move(candidate_->nodeEntities_);
        target.gizmoTranslation_=candidate_->gizmoTranslation_;target.gizmoRotation_=candidate_->gizmoRotation_;target.gizmoScale_=candidate_->gizmoScale_;
        target.clearAnimationPreview();target.dirty_=true;
        if(target.attachedRenderSettings_)*target.attachedRenderSettings_=target.scene_.renderSettings;
        target.mergeKey_=mergeKey;target.mergeRevision_=target.revision_;target.mergeSelection_=target.selectedNodes_;
    }else {
        target.selectedNodeIndex_=candidate_->selectedNodeIndex_;target.selectedNodes_=candidate_->selectedNodes_;
        target.animationPreview_=candidate_->animationPreview_;
        target.refreshSelectedTransform();++target.revision_;target.mergeKey_.clear();
    }
}
}
