#include "GizmoController.hpp"
#include "SelectionBounds.hpp"
#include "editor/commands/EditTransaction.hpp"
#include "scene/TransformEditing.hpp"
#include "scene/SceneDescription.hpp"
#include <set>
namespace azurerender {
using namespace internal;
Matrix4 GizmoController::pivotMatrix(const GizmoDragRequest& request) const {
    if(request.space!="world"&&request.space!="local")throw EditRejection("Unknown gizmo coordinate space");
    if(request.pivot!="active"&&request.pivot!="bounds")throw EditRejection("Unknown gizmo pivot");
    if(!context_.selectedNode())throw EditRejection("Select objects before using a gizmo");
    const auto description=context_.scene().renderDescription();
    const auto world=scene::resolveNodeWorldTransforms(description);
    const auto& active=world.at(context_.selectedNodeIndex());
    Matrix4 matrix=scene::identityMatrix();
    if(request.space=="local")for(unsigned column=0;column<3;++column){
        const Vector3 direction{active[column*4],active[column*4+1],active[column*4+2]};
        const auto length=vectorLength(direction);if(length<1e-7F)throw EditRejection("Local gizmo has a singular axis");
        for(unsigned row=0;row<3;++row)matrix[column*4+row]=direction[row]/length;
    }
    Vector3 pivot{active[12],active[13],active[14]};
    if(request.pivot=="bounds"){
        std::vector<std::string> ids;for(auto index:context_.selectedNodes())ids.push_back(description.nodes.at(index).id);
        const auto bounds=SelectionBounds::resolve(description,ids,[&](const std::string& resource)->std::optional<scene::AxisAlignedBounds>{
            const auto found=context_.selectionAssetBounds_.find(resource);return found==context_.selectionAssetBounds_.end()?std::optional<scene::AxisAlignedBounds>{}:found->second;
        });
        if(!bounds.valid)throw EditRejection(bounds.diagnostic);
        for(unsigned axis=0;axis<3;++axis)pivot[axis]=(bounds.bounds.minimum[axis]+bounds.bounds.maximum[axis])*.5F;
    }
    for(unsigned axis=0;axis<3;++axis)matrix[12+axis]=pivot[axis];
    return matrix;
}
nlohmann::json GizmoController::begin(const GizmoDragRequest& request) {
    if(active_)throw EditRejection("Finish the active drag before starting another");
    const auto pivot=pivotMatrix(request);(void)scene::inverseAffine(pivot);
    const auto description=context_.scene().renderDescription();const auto world=scene::resolveNodeWorldTransforms(description);
    std::set<std::string> selected;for(auto index:context_.selectedNodes())selected.insert(description.nodes.at(index).id);
    std::vector<std::size_t> roots;
    for(auto index:context_.selectedNodes()){
        auto parent=description.nodes.at(index).parentId;bool nested=false;
        while(!parent.empty()){
            if(selected.count(parent)){nested=true;break;}
            const auto found=std::find_if(description.nodes.begin(),description.nodes.end(),[&](const auto& node){return node.id==parent;});
            if(found==description.nodes.end())throw EditRejection("Missing parent in gizmo selection");parent=found->parentId;
        }
        if(!nested)roots.push_back(index);
    }
    initial_=context_.snapshot();undoBefore_=context_.undoStack_;redoBefore_=context_.redoStack_;
    startWorld_=world;roots_=std::move(roots);startPivot_=pivot;expected_=context_.documentVersion();
    mergeKey_="gizmo-"+newDocumentIdentity();active_=true;updated_=false;
    return {{"matrix",pivot},{"rootCount",roots_.size()}};
}
void GizmoController::checkVersion() const {
    if(!active_)throw EditRejection("No active gizmo drag");
    if(context_.documentVersion()!=expected_)throw EditRejection("Gizmo drag became stale");
}
void GizmoController::update(const GizmoDragUpdate& update) {
    checkVersion();(void)scene::inverseAffine(update.matrix);
    const auto delta=multiply(update.matrix,scene::inverseAffine(startPivot_));
    EditTransaction transaction(context_);auto& candidate=transaction.candidate();
    for(const auto index:roots_){
        const auto& source=initial_.scene.nodes.at(index);auto matrix=multiply(delta,startWorld_.at(index));
        if(!source.parentId.empty()){
            const auto parent=std::find_if(initial_.scene.nodes.begin(),initial_.scene.nodes.end(),[&](const auto& node){return node.id==source.parentId;});
            if(parent==initial_.scene.nodes.end())throw EditRejection("Missing gizmo parent");
            matrix=multiply(scene::inverseAffine(startWorld_.at(static_cast<std::size_t>(parent-initial_.scene.nodes.begin()))),matrix);
        }
        const auto value=scene::decomposeTrs(matrix,source.scale);
        auto& node=candidate.scene_.nodes.at(index);node.translation=value.translation;node.rotation=value.rotation;node.scale=value.scale;
    }
    // This controller owns the continuous edit, including across operation calls.
    if(updated_){context_.mergeKey_=mergeKey_;context_.mergeRevision_=expected_.revision;context_.mergeSelection_=initial_.selectedNodes;}
    const bool changed=candidate.documentContent()!=context_.documentContent();
    transaction.commit(mergeKey_);updated_=updated_||changed;expected_=context_.documentVersion();
}
void GizmoController::commit(){checkVersion();context_.closeEditMerge();active_=false;undoBefore_.clear();redoBefore_.clear();}
void GizmoController::cancel(){
    checkVersion();context_.restore(initial_);context_.undoStack_=std::move(undoBefore_);context_.redoStack_=std::move(redoBefore_);
    ++context_.revision_;context_.closeEditMerge();active_=false;
}
}
