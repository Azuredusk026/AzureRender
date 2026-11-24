#pragma once
#include "EditorContext.hpp"
namespace azurerender {
class PanelDocumentView {
public:
    explicit PanelDocumentView(const EditorContext& context):context_(context){}
    const SceneDocument& scene() const {return context_.scene();}
    const SceneNode* selectedNode() const {return context_.selectedNode();}
    std::size_t selectedNodeIndex() const {return context_.selectedNodeIndex();}
    const std::vector<std::size_t>& selectedNodes() const {return context_.selectedNodes();}
    const RenderSettings& renderSettings() const {return context_.renderSettings();}
    const std::array<float,3>& gizmoTranslation() const {return context_.gizmoTranslation();}
    const std::array<float,3>& gizmoRotation() const {return context_.gizmoRotation();}
    const std::array<float,3>& gizmoScale() const {return context_.gizmoScale();}
    bool isProject() const {return context_.isProject();}
    const Project& project() const {return context_.project();}
    const AssetDatabase& assets() const {return context_.assets();}
    std::vector<EditorContext::ResourceStatus> resourceStatuses() const {return context_.resourceStatuses();}
    nlohmann::json componentData(const std::string& node,const std::string& type) const {return context_.componentData(node,type);}
    bool importing() const {return context_.importing();}
    float importProgress() const {return context_.importProgress();}
    const nlohmann::json& importSummary() const {return context_.importSummary();}
    const std::optional<NodeAnimationFrame>& animationPreview() const {return context_.animationPreview();}
private:const EditorContext& context_;
};
}
