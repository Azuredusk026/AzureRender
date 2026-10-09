#pragma once

#include "EditorCameraController.hpp"
#include "EditorSession.hpp"
#include "runtime/GameUi.hpp"
#include "IEditorPanel.hpp"
#include "EditorWorkspace.hpp"

#include <vulkan/vulkan.h>

#ifdef AZURERENDER_HAS_IMGUI
#include <imgui.h>
#endif

#include <array>
#include <memory>
#include <vector>
#include <map>
#include <set>

struct GLFWwindow;

namespace azurerender {

class ImGuiEditorLayer final {
public:
    explicit ImGuiEditorLayer(std::shared_ptr<EditorSession> session);
    ImGuiEditorLayer(const ImGuiEditorLayer&) = delete;
    ImGuiEditorLayer& operator=(const ImGuiEditorLayer&) = delete;
    ~ImGuiEditorLayer();

    void initialize(
        GLFWwindow* window,
        VkInstance instance,
        VkPhysicalDevice physicalDevice,
        VkDevice device,
        std::uint32_t queueFamily,
        VkQueue queue,
        VkRenderPass renderPass,
        std::uint32_t imageCount, VkFormat colorFormat=VK_FORMAT_B8G8R8A8_SRGB);
    void shutdownVulkan();
    void newFrame();
    void drawPanels();
    void completePreviewTextures(std::uint64_t completed);
    void setPreviewSubmission(std::uint64_t submission){previewSubmission_=submission;}
    nlohmann::json workspaceSnapshot(bool includeHistory = true) const;
    void queueInputEvent(nlohmann::json event);
    void setCameraState(std::array<float,3> position,std::array<float,3> target){cameraPosition_=position;cameraTarget_=target;}
    void setGameUi(GameUi* ui) { gameUi_=ui; }
    std::uint64_t debugLineCount() const noexcept { return debugLineCount_; }
    void render(VkCommandBuffer commandBuffer);
    void setViewportImages(
        VkSampler sampler,
        const std::vector<VkImageView>& imageViews,
        std::uint32_t width,
        std::uint32_t height);
    void clearViewportImages();
    void setViewportImageIndex(std::uint32_t imageIndex);
    [[nodiscard]] EditorViewportInput consumeViewportInput() noexcept;
    bool consumeViewportResizeRequest(
        std::uint32_t& width,
        std::uint32_t& height) noexcept;
    [[nodiscard]] bool acceptsViewportShortcuts() const noexcept {
        return viewportAcceptsShortcuts_;
    }

private:
    struct PreviewTexture {VkDescriptorSet texture=VK_NULL_HANDLE;std::uint64_t lastUse=0;};
    std::map<std::uint64_t,PreviewTexture> previewTextures_;
    std::uint64_t previewSubmission_=0;
    VkDescriptorSet previewTexture(std::uint64_t handle);
    bool cameraPreviewEnabled_=false;
    std::array<char,4096> proposalInstruction_{};
    int proposalDomain_=0;
    std::uint64_t proposalSequence_=0;
    void drawSettingsPanel(PanelContext& context);
    void drawProjectBrowserPanel(PanelContext& context);
    void drawEnvironmentPanel(PanelContext& context);
    std::string projectOpenPath_,projectDestination_,projectName_="New Project";
    int projectTemplate_=0,settingsCategory_=0;

#ifdef AZURERENDER_HAS_IMGUI
    void cancelViewportGizmo();
    bool drawViewportGizmo(ImVec2 origin,ImVec2 size);
    void drawPathInput(const char* label,std::string& value,const std::string& purpose,bool directory,const std::vector<std::string>& extensions={});
#endif
    std::array<float,16> viewportGizmoMatrix_{};
    void drawViewportPanel(PanelContext& context);
    void drawOutlinerPanel(PanelContext& context);
    void drawInspectorPanel(PanelContext& context);
    void drawAssetBrowserPanel(PanelContext& context);
    void drawCapturePanel(PanelContext& context);
    void drawConsolePanel(PanelContext& context);
    void drawBuildPanel(PanelContext& context);
    void drawAnimationPanel(PanelContext& context);
    void drawGameplayDebugPanel(PanelContext& context);
    void observeWidget(const std::string& id);
    void injectUiEvents();
    void drawWorkspace();

    std::shared_ptr<EditorSession> session_;
    EditorContext* context_ = nullptr;
    GameUi* gameUi_=nullptr;
    std::vector<std::unique_ptr<IEditorPanel>> panels_;
    VkDevice device_ = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> viewportTextures_;
    std::uint32_t viewportImageIndex_ = 0;
    std::uint32_t viewportWidth_ = 1;
    std::uint32_t viewportHeight_ = 1;
    std::uint32_t resizeCandidateWidth_ = 0;
    std::uint32_t resizeCandidateHeight_ = 0;
    std::uint32_t resizeStableFrames_ = 0;
    bool viewportResizePending_ = false;
    EditorViewportInput viewportInput_;
    std::int32_t gizmoDragAxis_ = -1;
#ifdef AZURERENDER_HAS_IMGUI
    ImVec2 gizmoDragStartMouse_{0.0F, 0.0F};
#endif
    std::array<float, 3> gizmoDragStartTranslation_{0.0F, 0.0F, 0.0F};
    int navigationButton_=-1;
    int injectedClickButton_=0;
    std::array<float,3> cameraPosition_{},cameraTarget_{};
    bool viewportGizmoDragActive_ = false;
    bool viewportFocused_ = false;
    bool viewportAcceptsShortcuts_ = false;
    bool initialized_ = false;
    std::uint64_t debugLineCount_ = 0;
    bool dockingLayoutInitialized_ = false;
    bool workspaceRebuildRequested_ = false;
    EditorWorkspace workspace_;
    std::filesystem::path configDirectory_;
    std::string iniPath_;
    std::vector<std::uint32_t> uiFragmentCode_;
    GLFWwindow* window_ = nullptr;
    float dpi_ = 1, dpiOverride_ = 0;
    std::uint64_t uiFrame_ = 0;
    nlohmann::json uiActions_=nlohmann::json::array(),uiHistory_=nlohmann::json::array();
    nlohmann::json uiErrors_=nlohmann::json::array(),widgets_=nlohmann::json::object();
    std::size_t uiCursor_ = 0;
    bool injectedMouseDown_ = false;
    bool injectedFocus_=true;
    std::array<float,2> uiMousePosition_{0,0};
    std::array<float,4> imageRect_{};
    std::array<float,3> gizmoDragStartRotation_{},gizmoDragStartScale_{};
#ifdef AZURERENDER_HAS_IMGUI
    ImGuiTextFilter settingsFilter_;
#endif
    int settingSourceIndex_=0;
    std::string settingDiagnostic_;
    bool settingsSaveRequested_=false,compactPreference_=false;
#ifdef AZURERENDER_HAS_IMGUI
    ImGuiTextFilter outlinerFilter_,assetFilter_,consoleFilter_,referenceFilter_;
#endif
    int consoleLevel_ = 0;
    bool assetGrid_ = false;
    std::string assetDirectory_;
    std::string assetTypeId_,importPath_,prefabInstance_,installPath_,outputPath_,projectPath_;
    std::set<std::string> openNodeIds_;
    nlohmann::json visibleAssets_=nlohmann::json::array();
};

}  // namespace azurerender
