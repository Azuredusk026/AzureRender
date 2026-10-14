#include "editor/panels/CorePanels.hpp"
#include "ui/BrandMark.hpp"
#include "platform/WindowIcon.hpp"
#include "runtime/InputPreferences.hpp"
#include "ImGuiEditorLayer.hpp"
#include "render/RenderMath.hpp"
#include "render/CameraProjection.hpp"
#include <cstdio>
#include "input/EditorInputRouter.hpp"
#include "EditorTheme.hpp"
#include "panels/UiCatalogPanel.hpp"
#include "EditorToolbar.hpp"
#include "resources/ResourceLocator.hpp"
#include "foundation/Environment.hpp"
#include <GLFW/glfw3.h>
#include <cstdlib>
#include "reflection/Registry.hpp"
#include "runtime/ComponentRegistry.hpp"
#include "ecs/Components.hpp"
#include "diagnostics/RuntimeDiagnostics.hpp"
#include "extensions/ExtensionRegistry.hpp"
#include "editor/GameplayDebugGeometry.hpp"

#ifdef AZURERENDER_HAS_IMGUI
#include <imgui.h>
#include "ImGuizmo.h"
#ifdef IMGUI_HAS_DOCK
#include <imgui_internal.h>
#endif
#if __has_include(<backends/imgui_impl_glfw.h>)
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
#else
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <functional>
#include <fstream>
#include <stdexcept>
#include <utility>
#include <set>
#endif

namespace azurerender {

#ifdef AZURERENDER_HAS_IMGUI

namespace {

#ifndef IMGUI_HAS_DOCK
void setFallbackPanelRect(
    const float x,
    const float y,
    const float width,
    const float height) {
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos({display.x * x, display.y * y}, ImGuiCond_Always);
    ImGui::SetNextWindowSize(
        {display.x * width, display.y * height}, ImGuiCond_Always);
}
#endif

class CallbackEditorPanel final : public IEditorPanel {
public:
    CallbackEditorPanel(
        const std::string_view id,
        const std::string_view title,
        std::function<void(PanelContext&)> draw)
        : id_(id), title_(title), draw_(std::move(draw)) {}

    [[nodiscard]] std::string_view id() const noexcept override { return id_; }
    [[nodiscard]] std::string_view title() const noexcept override {
        return title_;
    }
    void draw(PanelContext& context) override { draw_(context); }

private:
    std::string_view id_;
    std::string_view title_;
    std::function<void(PanelContext&)> draw_;
};

}  // namespace

ImGuiEditorLayer::ImGuiEditorLayer(std::shared_ptr<EditorSession> session)
    : session_(std::move(session)) {
    if (session_ == nullptr) {
        throw std::invalid_argument("ImGui editor requires an editor session");
    }
    context_ = &session_->context();
    installPath_=ResourceLocator{}.installationRoot().u8string();
    if(!context_->isProject())workspace_.setVisible("projects",true);
    EditorPanelRegistry registry;
    const auto addPanel = [&registry](
                              const char* id,
                              const char* title,
                              std::function<void(PanelContext&)> draw) {
        registry.registerFactory(
            {std::string("panel.") + id, 1, {"editor.panel"}, {}},
            [id, title, draw = std::move(draw)] {
                return std::make_unique<CallbackEditorPanel>(
                    id, title, draw);
            });
    };
    addPanel("viewport", "Viewport", [this](PanelContext& context) { drawViewportPanel(context); });
    addPanel("capture", "Capture", [this](PanelContext& context) { drawCapturePanel(context); });
    addPanel("console", "Console", [this](PanelContext& context) { drawConsolePanel(context); });
    addPanel("build", "Build Game", [this](PanelContext& context) { drawBuildPanel(context); });
    addPanel("animation", "Animation Preview", [this](PanelContext& context) { drawAnimationPanel(context); });
    addPanel("gameplay-debug", "Gameplay Debug", [this](PanelContext& context) { drawGameplayDebugPanel(context); });
    addPanel("settings", "Settings", [this](PanelContext& context) { drawSettingsPanel(context); });
    addPanel("projects", "Projects", [this](PanelContext& context) { drawProjectBrowserPanel(context); });
    addPanel("environment", "Environment", [this](PanelContext& context) { drawEnvironmentPanel(context); });
    workspace_.registerPanel("scripts","Script Authoring",false);
    workspace_.registerPanel("problems","Problems",false);
    workspace_.registerPanel("assistance","Content Assistance",false);
    workspace_.registerPanel("prefabs","Prefab Authoring",false);
    addPanel("prefabs", "Prefab Authoring", [this](PanelContext& context) { drawPrefabAuthoringPanel(context); });
    addPanel("assistance", "Content Assistance", [this](PanelContext& context) { drawContentAssistancePanel(context); });
    addPanel("scripts", "Script Authoring", [this](PanelContext& context) { drawScriptAuthoringPanel(context); });
    addPanel("problems", "Problems", [this](PanelContext& context) { drawProblemsPanel(context); });
    workspace_.registerPanel("ui-catalog","控件目录",false);
    addPanel("ui-catalog", "控件目录", [](PanelContext&) { drawUiCatalog(); });
    CorePanelServices services{*session_,workspace_,dpi_,widgets_,
        [this](const auto& id){observeWidget(id);},
        [this]{inspectorPanel_->presentation.focusObjectName_=true;},
        [this](nlohmann::json request){assetPanel_->presentation.exportRequest_=std::move(request);assetPanel_->presentation.exportDialogRequested_=true;},
        [this](const auto& node){drawMaterialSlots(node);},[this](const auto& node){drawPhysicsAuthoring(node);},
        [this]{drawTextureAssetPanel();},[this]{drawMaterialAssetPanel();},
        [this](auto handle){return previewTexture(handle);},
        [this](const auto* label,auto& value,const auto& purpose,bool directory,const auto& extensions){drawPathInput(label,value,purpose,directory,extensions);}};
    registry.registerFactory({"panel.outliner",1,{"editor.panel"},{}},[this,services]{auto panel=std::make_unique<OutlinerPanel>(services);outlinerPanel_=panel.get();return panel;});
    registry.registerFactory({"panel.inspector",1,{"editor.panel"},{}},[this,services]{auto panel=std::make_unique<InspectorPanel>(services);inspectorPanel_=panel.get();return panel;});
    registry.registerFactory({"panel.assets",1,{"editor.panel"},{}},[this,services]{auto panel=std::make_unique<AssetBrowserPanel>(services);assetPanel_=panel.get();return panel;});
    panels_=registry.createAll();
    auto extensions=session_->panelRegistry().createAll();
    for(auto& panel:extensions) {
        workspace_.registerPanel(std::string(panel->id()),std::string(panel->title()));
        panels_.push_back(std::move(panel));
    }
}

ImGuiEditorLayer::~ImGuiEditorLayer() {
    shutdownVulkan();
}

void ImGuiEditorLayer::initialize(
    GLFWwindow* window,
    const VkInstance instance,
    const VkPhysicalDevice physicalDevice,
    const VkDevice device,
    const std::uint32_t queueFamily,
    const VkQueue queue,
    const VkRenderPass renderPass,
    const std::uint32_t imageCount, const VkFormat colorFormat) {
    if (initialized_) {
        shutdownVulkan();
    }
    device_ = device;
    dockingLayoutInitialized_ = false;
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
#ifdef IMGUI_HAS_DOCK
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
#endif
    window_=window;
    setWindowIcon(window_,64,ui::brandPixels(64));
    configDirectory_=EditorWorkspace::configDirectory();
    try{session_->setPathHistoryFile(configDirectory_/"path-history.json");}catch(const std::exception& error){session_->log(std::string("Path history: ")+error.what());}
    const bool restored=workspace_.load(configDirectory_);
    for(const auto& [category,policy]:workspace_.overlayPolicies)if(session_->viewportOverlays().contains(category))session_->viewportOverlayPolicies()[category]=policy;
    std::filesystem::create_directories(configDirectory_);
    iniPath_=(configDirectory_/"layout.ini").u8string();
    io.IniFilename=nullptr;
    if(restored && std::filesystem::exists(configDirectory_/"layout.ini")) {
        std::ifstream file(configDirectory_/"layout.ini");std::string contents((std::istreambuf_iterator<char>(file)),{});
        if(contents.find("[Docking][Data]")!=std::string::npos)ImGui::LoadIniSettingsFromMemory(contents.c_str());
        else { workspace_.diagnostic="Invalid docking data. Default layout loaded.";workspace_.reset(); }
    }
    if(!workspace_.diagnostic.empty())context_->log(workspace_.diagnostic);
    if(const auto* value=std::getenv("AZURERENDER_EDITOR_DPI"))dpiOverride_=std::stof(value);
    float scaleY=1;glfwGetWindowContentScale(window_,&dpi_,&scaleY);if(dpiOverride_>0)dpi_=dpiOverride_;
    dpi_=std::clamp(dpi_*session_->settings().get("editor.scale").get<float>(),.75F,3.F);
    EditorTheme::apply(dpi_);
    const auto font=ResourceLocator{}.publicAsset("fonts/NotoSansCJKsc-Regular.otf");
    if(io.Fonts->AddFontFromFileTTF(font.u8string().c_str(),14)==nullptr)throw std::runtime_error("Editor font could not be loaded");
    if(uiFrame_==0)if(const auto path=environmentPath("AZURERENDER_EDITOR_UI_ACTIONS")) {
        std::ifstream file(*path);file>>uiActions_;
        if(!uiActions_.is_array())throw std::invalid_argument("UI actions must be an array");
    }

    if (!ImGui_ImplGlfw_InitForVulkan(window, true)) {
        ImGui::DestroyContext();
        throw std::runtime_error("ImGui GLFW backend initialization failed");
    }

    constexpr std::array<VkDescriptorPoolSize, 11> poolSizes = {{
        {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000},
    }};
    const VkDescriptorPoolCreateInfo poolInfo{
        VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        nullptr,
        VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        1000 * static_cast<std::uint32_t>(poolSizes.size()),
        static_cast<std::uint32_t>(poolSizes.size()),
        poolSizes.data(),
    };
    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool_)
        != VK_SUCCESS) {
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        throw std::runtime_error("ImGui descriptor pool creation failed");
    }

    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.ApiVersion = VK_API_VERSION_1_3;
    initInfo.Instance = instance;
    initInfo.PhysicalDevice = physicalDevice;
    initInfo.Device = device;
    initInfo.QueueFamily = queueFamily;
    initInfo.Queue = queue;
    initInfo.DescriptorPool = descriptorPool_;
    initInfo.MinImageCount = 2;
    initInfo.ImageCount = imageCount;
    initInfo.PipelineInfoMain.RenderPass = renderPass;
    initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    const bool srgb=colorFormat==VK_FORMAT_B8G8R8A8_SRGB || colorFormat==VK_FORMAT_R8G8B8A8_SRGB;
    const auto shader=ResourceLocator{}.shaderDirectory()/(srgb?"editor_ui_srgb.frag.spv":"editor_ui.frag.spv");
    std::ifstream bytecode(shader,std::ios::binary|std::ios::ate);
    const auto bytes=bytecode.tellg();
    if(bytes<=0 || bytes%4!=0)throw std::runtime_error("Editor UI shader is invalid");
    uiFragmentCode_.resize(static_cast<std::size_t>(bytes)/4);bytecode.seekg(0);
    bytecode.read(reinterpret_cast<char*>(uiFragmentCode_.data()),bytes);
    initInfo.CustomShaderFragCreateInfo.sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    initInfo.CustomShaderFragCreateInfo.codeSize=uiFragmentCode_.size()*4;
    initInfo.CustomShaderFragCreateInfo.pCode=uiFragmentCode_.data();
    if (!ImGui_ImplVulkan_Init(&initInfo)) {
        vkDestroyDescriptorPool(device, descriptorPool_, nullptr);
        descriptorPool_ = VK_NULL_HANDLE;
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        throw std::runtime_error("ImGui Vulkan backend initialization failed");
    }
    initialized_ = true;
}

void ImGuiEditorLayer::shutdownVulkan() {
    if (!initialized_) {
        return;
    }
    setNavigationCapture(false);
    workspace_.overlayPolicies=session_->viewportOverlayPolicies();
    try { ImGui::SaveIniSettingsToDisk(iniPath_.c_str());workspace_.save(configDirectory_); }
    catch(const std::exception& error){context_->log(std::string("Workspace: ")+error.what());}
    for (const VkDescriptorSet texture : viewportTextures_) {
        ImGui_ImplVulkan_RemoveTexture(texture);
    }
    viewportTextures_.clear();
    for(const auto& entry:previewTextures_)ImGui_ImplVulkan_RemoveTexture(entry.second.texture);
    previewTextures_.clear();
    ImGui_ImplVulkan_Shutdown();
    if (descriptorPool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device_, descriptorPool_, nullptr);
        descriptorPool_ = VK_NULL_HANDLE;
    }
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    device_ = VK_NULL_HANDLE;
    initialized_ = false;
}

void ImGuiEditorLayer::newFrame(float fixedDeltaSeconds) {
    if (!initialized_) {
        return;
    }
    session_->pollModel();
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    if(fixedDeltaSeconds>0)ImGui::GetIO().DeltaTime=fixedDeltaSeconds;
    float nextDpi=1,scaleY=1;glfwGetWindowContentScale(window_,&nextDpi,&scaleY);
    session_->settings().applyPending();
    const auto compact=session_->settings().get("editor.compact").get<bool>();
    if(compactPreference_!=compact){compactPreference_=compact;workspaceRebuildRequested_=true;dockingLayoutInitialized_=false;}
    if(settingsSaveRequested_) {
        settingsSaveRequested_=false;
        try{session_->settings().saveUser(session_->userSettingsPath().empty()?configDirectory_/"settings.json":session_->userSettingsPath());settingDiagnostic_.clear();}
        catch(const std::exception& error){settingDiagnostic_=error.what();}
    }
    if(dpiOverride_>0)nextDpi=dpiOverride_;
    nextDpi=std::clamp(nextDpi*session_->settings().get("editor.scale").get<float>(),.75F,3.F);
    if(std::abs(nextDpi-dpi_)>.01F){dpi_=nextDpi;EditorTheme::apply(dpi_);dockingLayoutInitialized_=false;workspaceRebuildRequested_=true;}
    ++uiFrame_;injectUiEvents();
    ImGui::NewFrame();
    ImGuizmo::BeginFrame();
}

void ImGuiEditorLayer::drawPanels() {
    if (!initialized_) {
        return;
    }
    context_=&session_->context();
    widgets_=nlohmann::json::object();
    EditorToolbar::draw(*session_,workspace_,dpi_,[this](const std::string& id){observeWidget(id);});
    drawWorkspace();
    const ImGuiIO& io = ImGui::GetIO();
    viewportFocused_=false;viewportAcceptsShortcuts_=false;
    const bool nativeFocus=!io.AppFocusLost;
    if(session_->references().active()&&(!nativeFocus||ImGui::IsKeyPressed(ImGuiKey_Escape,false)))session_->edit("reference.cancel");
    if(!nativeFocus || !workspace_.visible("viewport")) {
        cancelViewportGizmo();navigationButton_=-1;viewportInput_={};
        setNavigationCapture(false);
    }
    for (const std::unique_ptr<IEditorPanel>& panel : panels_) {
        if(!workspace_.visible(std::string(panel->id())))continue;
        const bool editable=std::string(panel->id())=="viewport" || std::string(panel->id())=="console" || std::string(panel->id())=="capture";
        ImGui::BeginDisabled((session_->playing() || session_->building()) && !editable && panel->id()!="build" && panel->id()!="settings");auto panelContext=session_->panelContext();panel->draw(panelContext);ImGui::EndDisabled();
    }
    drawAssetTransferPanel();
    processFileDrops();
    const auto* nav=ImGui::GetCurrentContext()->NavWindow;
    const bool sceneFocus=viewportFocused_ || (nav && std::string(nav->Name).find("###outliner")!=std::string::npos);
    const bool modal=ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId|ImGuiPopupFlags_AnyPopupLevel);
    std::map<std::string,std::string> bindings;
    for(const auto& entry:std::vector<std::pair<const char*,const char*>>{{"W","move"},{"E","rotate"},{"R","scale"},{"Q","select"},{"F","frame"}})
        bindings[entry.first]=session_->settings().get(std::string("editor.shortcuts.")+entry.second).get<std::string>();
    for(int key=ImGuiKey_NamedKey_BEGIN;key<ImGuiKey_NamedKey_END;++key) {
        const auto imguiKey=static_cast<ImGuiKey>(key);
        if(!ImGui::IsKeyPressed(imguiKey,false))continue;
        EditorInputEvent event;event.key=ImGui::GetKeyName(imguiKey);event.ctrl=io.KeyCtrl;
        event.text=io.WantTextInput || ImGui::IsAnyItemActive();event.modal=modal;
        event.scene=sceneFocus;event.viewport=viewportFocused_;event.navigating=navigationButton_>=0;
        event.playing=session_->playing();event.simulating=session_->simulating();event.building=session_->building();
        const auto routed=EditorInputRouter::route(event,bindings);
        if(routed.consumed)session_->edit(routed.operation,routed.mode>=0?nlohmann::json{{"value",routed.mode}}:nlohmann::json::object());
    }
    auto& guard=session_->documentGuard();
    if(guard.state()==DocumentActionState::AwaitingDecision || guard.state()==DocumentActionState::Failed)ImGui::OpenPopup("Unsaved changes");
    if(ImGui::BeginPopupModal("Unsaved changes",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted(guard.action()==DocumentAction::Close?"Save changes before closing?":guard.action()==DocumentAction::Open?"Save changes before switching project?":"Save changes before reloading?");
        if(!guard.diagnostic().empty())ImGui::TextWrapped("%s",guard.diagnostic().c_str());
        for(const auto* decision:{"save","discard","cancel"}) {
            if(ImGui::Button(decision)) {session_->edit("document.decision",{{"value",decision}});if(guard.state()!=DocumentActionState::Failed)ImGui::CloseCurrentPopup();}
            observeWidget(std::string("document.")+decision);ImGui::SameLine();
        }
        ImGui::NewLine();ImGui::EndPopup();
    }
    EditorToolbar::status(*session_,dpi_);
    if(!ImGui::IsAnyItemActive()&&!viewportGizmoDragActive_)context_->closeEditMerge();
#ifdef IMGUI_HAS_DOCK
    for(const auto& panel:workspace_.panels())if(const auto* window=ImGui::FindWindowByName(panel.title.c_str())) {
        widgets_["panelrect."+panel.id]={window->Pos.x,window->Pos.y,window->Size.x,window->Size.y};
        if(window->DockNode && window->DockNode->TabBar) {
            const auto* bar=window->DockNode->TabBar;
            for(const auto& tab:bar->Tabs)if(tab.Window==window)
                {
                widgets_["tab."+panel.id]={bar->BarRect.Min.x+tab.Offset,bar->BarRect.Min.y,tab.Width,bar->BarRect.GetHeight()};
                widgets_["tool."+panel.id]=widgets_["tab."+panel.id];
                }
        }
    }
#endif
    if(!uiActions_.empty()&&uiHistory_.size()<8192)uiHistory_.push_back({{"frame",uiFrame_},{"playing",session_->playing()},
        {"paused",session_->runtime()&&session_->runtime()->state()==RuntimeLifecycle::State::Paused},
        {"selected",context_->selectedNode()?context_->selectedNode()->id:""},{"dirty",context_->dirty()},
        {"nodeCount",context_->scene().nodes.size()},{"translation",context_->gizmoTranslation()},
        {"rotation",context_->gizmoRotation()},{"scale",context_->gizmoScale()},
        {"gizmoMode",static_cast<unsigned>(context_->gizmoMode())},{"camera",cameraPosition_},{"target",cameraTarget_},
        {"navigationButton",navigationButton_},{"gizmoCapture",viewportGizmoDragActive_},{"undoCount",context_->undoCount()},
        {"cursorCaptured",navigationCursorCaptured_},{"flySpeed",session_->settings().get("editor.camera.flySpeed")},
        {"mouse",{io.MousePos.x,io.MousePos.y}},{"mouseDown",io.MouseDown[0]},
        {"activeId",ImGui::GetCurrentContext()->ActiveId},
        {"movingWindow",ImGui::GetCurrentContext()->MovingWindow?ImGui::GetCurrentContext()->MovingWindow->Name:""}});
    if(uiFrame_%12==0)if(const auto path=environmentPath("AZURERENDER_UI_OBSERVATION")) {
        auto snapshot=workspaceSnapshot(false);snapshot["phase"]="Editor";snapshot["frame"]=uiFrame_;
        std::ofstream output(*path);output<<snapshot.dump();
    }
}
void ImGuiEditorLayer::completePreviewTextures(std::uint64_t completed){
    for(auto it=previewTextures_.begin();it!=previewTextures_.end();){
        if(it->second.lastUse<=completed){if(initialized_)ImGui_ImplVulkan_RemoveTexture(it->second.texture);it=previewTextures_.erase(it);}else ++it;
    }
}
VkDescriptorSet ImGuiEditorLayer::previewTexture(std::uint64_t handle){
    if(!session_->developerServices().image)return VK_NULL_HANDLE;
    const auto image=session_->developerServices().image(handle);if(!image.image)return VK_NULL_HANDLE;
    auto& texture=previewTextures_[handle];
    if(!texture.texture)texture.texture=ImGui_ImplVulkan_AddTexture(image.sampler,image.image,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    texture.lastUse=previewSubmission_;return texture.texture;
}

void ImGuiEditorLayer::render(const VkCommandBuffer commandBuffer) {
    if (!initialized_) {
        return;
    }
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
}

void ImGuiEditorLayer::setViewportImages(
    const VkSampler sampler,
    const std::vector<VkImageView>& imageViews,
    const std::uint32_t width,
    const std::uint32_t height) {
    if (!initialized_) {
        return;
    }
    clearViewportImages();
    viewportTextures_.reserve(imageViews.size());
    for (const VkImageView imageView : imageViews) {
        viewportTextures_.push_back(ImGui_ImplVulkan_AddTexture(
            sampler,
            imageView,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
    }
    viewportWidth_ = std::max(width, 1U);
    viewportHeight_ = std::max(height, 1U);
    viewportImageIndex_ = 0;
}

void ImGuiEditorLayer::clearViewportImages() {
    if (!initialized_) {
        viewportTextures_.clear();
        return;
    }
    for (const VkDescriptorSet texture : viewportTextures_) {
        ImGui_ImplVulkan_RemoveTexture(texture);
    }
    viewportTextures_.clear();
    viewportImageIndex_ = 0;
}

void ImGuiEditorLayer::setViewportImageIndex(
    const std::uint32_t imageIndex) {
    if (imageIndex < viewportTextures_.size()) {
        viewportImageIndex_ = imageIndex;
    }
}

EditorViewportInput ImGuiEditorLayer::consumeViewportInput() noexcept {
    EditorViewportInput input = viewportInput_;
    viewportInput_ = {};
    return input;
}

bool ImGuiEditorLayer::consumeViewportResizeRequest(
    std::uint32_t& width,
    std::uint32_t& height) noexcept {
    if (!viewportResizePending_) {
        return false;
    }
    width = resizeCandidateWidth_;
    height = resizeCandidateHeight_;
    viewportResizePending_ = false;
    resizeStableFrames_ = 0;
    return true;
}

void ImGuiEditorLayer::setNavigationCapture(bool captured){
    if(captured==navigationCursorCaptured_||!window_)return;
    if(captured){glfwGetCursorPos(window_,&savedNavigationCursor_[0],&savedNavigationCursor_[1]);navigationRawMouse_=session_->settings().get("editor.camera.rawMouse").get<bool>()&&glfwRawMouseMotionSupported()==GLFW_TRUE;
        glfwSetInputMode(window_,GLFW_CURSOR,GLFW_CURSOR_DISABLED);if(navigationRawMouse_)glfwSetInputMode(window_,GLFW_RAW_MOUSE_MOTION,GLFW_TRUE);
    }else{if(navigationRawMouse_)glfwSetInputMode(window_,GLFW_RAW_MOUSE_MOTION,GLFW_FALSE);glfwSetInputMode(window_,GLFW_CURSOR,GLFW_CURSOR_NORMAL);
        if(glfwGetWindowAttrib(window_,GLFW_FOCUSED))glfwSetCursorPos(window_,savedNavigationCursor_[0],savedNavigationCursor_[1]);
    }
    navigationCursorCaptured_=captured;
}

}  // namespace azurerender
#else

ImGuiEditorLayer::ImGuiEditorLayer(std::shared_ptr<EditorSession> session)
    : session_(std::move(session)) {
    if (session_ != nullptr) {
        context_ = &session_->context();
    }
}

ImGuiEditorLayer::~ImGuiEditorLayer() = default;

void ImGuiEditorLayer::initialize(
    GLFWwindow*, VkInstance, VkPhysicalDevice, VkDevice, std::uint32_t,
    VkQueue, VkRenderPass, std::uint32_t, VkFormat) {}

void ImGuiEditorLayer::shutdownVulkan() {}
void ImGuiEditorLayer::newFrame(float) {}
void ImGuiEditorLayer::drawPanels() {}
void ImGuiEditorLayer::completePreviewTextures(std::uint64_t) {}
void ImGuiEditorLayer::render(VkCommandBuffer) {}
void ImGuiEditorLayer::setViewportImages(
    VkSampler, const std::vector<VkImageView>&, std::uint32_t, std::uint32_t) {}
void ImGuiEditorLayer::clearViewportImages() {}
void ImGuiEditorLayer::setViewportImageIndex(std::uint32_t) {}
EditorViewportInput ImGuiEditorLayer::consumeViewportInput() noexcept {
    return {};
}
bool ImGuiEditorLayer::consumeViewportResizeRequest(
    std::uint32_t&, std::uint32_t&) noexcept {
    return false;
}

}  // namespace azurerender
#endif
