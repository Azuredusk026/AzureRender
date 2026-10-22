#pragma once
#include "editor/IEditorPanel.hpp"
#include "editor/EditorSession.hpp"
#include "editor/EditorWorkspace.hpp"
#include <imgui.h>
#include <vulkan/vulkan.h>
#include <functional>
#include <set>
namespace azurerender {
// Borrowed host capabilities; panel objects own their presentation state.
struct CorePanelServices {
    EditorSession& session;
    EditorWorkspace& workspace;
    float& dpi;
    nlohmann::json& widgets;
    std::function<void(const std::string&)> observe;
    std::function<void()> rename;
    std::function<void(nlohmann::json)> exportSelection;
    std::function<void(const SceneNode&)> materialSlots,physics;
    std::function<void()> textureDialog,materialDialog;
    std::function<VkDescriptorSet(std::uint64_t)> previewTexture;
    std::function<void(const char*,std::string&,const std::string&,bool,const std::vector<std::string>&)> pathInput;
};
struct OutlinerPanelState { ImGuiTextFilter outlinerFilter_;std::set<std::string> openNodeIds_; };
struct InspectorPanelState { ImGuiTextFilter referenceFilter_,componentFilter_,propertyFilter_;bool focusObjectName_=false; };
struct AssetPanelState {
    ImGuiTextFilter assetFilter_;
    nlohmann::json contentView_=nlohmann::json::object();
    bool assetGrid_ = false;
    std::string assetDirectory_;
    std::string assetMount_;
    std::string assetNavigationDiagnostic_;
    std::vector<std::pair<std::string,std::string>> assetNavigation_={{"",""}};
    std::size_t assetNavigationIndex_=0;
    std::uint64_t assetPreviewHandle_=0;
    std::string assetPreviewTitle_;
    bool assetPreviewRequested_=false;
    std::string primitiveKind_="cube",primitiveName_="Primitive.glb";
    std::string materialName_="Material.azurematerial",materialAsset_;
    bool materialDialogRequested_=false;
    nlohmann::json materialSnapshot_;
    std::string materialDraftAsset_,materialSurfaceDraft_,materialImagesDraft_,materialDraftError_;
    std::array<float,3> primitiveSize_{1,1,1};
    int primitiveSegments_=12;
    std::set<std::string> selectedAssets_,textureFields_;
    nlohmann::json exportRequest_=nlohmann::json::object(),exportImpact_;
    std::string exportFormat_,exportDestination_,exportTask_;
    bool exportDialogRequested_=false,exportReplace_=false;
    nlohmann::json textureSnapshot_,contentImpact_,contentRequest_,dependencySnapshot_;
    std::string contentValue_,contentOperation_;
    bool contentDialogRequested_=false,textureDialogRequested_=false,dependencyDialogRequested_=false;
    std::string assetTypeId_,importPath_;
    nlohmann::json visibleAssets_=nlohmann::json::array();
};
class OutlinerPanel final : public IEditorPanel {
public:
    explicit OutlinerPanel(CorePanelServices services):services_(std::move(services)){}
    std::string_view id() const noexcept override { return "outliner"; }
    std::string_view title() const noexcept override { return "场景层级"; }
    void draw(PanelContext&) override;
    OutlinerPanelState presentation;
private: CorePanelServices services_;
};
class InspectorPanel final : public IEditorPanel {
public:
    explicit InspectorPanel(CorePanelServices services):services_(std::move(services)){}
    std::string_view id() const noexcept override { return "inspector"; }
    std::string_view title() const noexcept override { return "属性"; }
    void draw(PanelContext&) override;
    InspectorPanelState presentation;
private: CorePanelServices services_;
};
class AssetBrowserPanel final : public IEditorPanel {
public:
    explicit AssetBrowserPanel(CorePanelServices services):services_(std::move(services)){}
    std::string_view id() const noexcept override { return "assets"; }
    std::string_view title() const noexcept override { return "资源"; }
    void draw(PanelContext&) override;
    AssetPanelState presentation;
private: CorePanelServices services_;
};
}
