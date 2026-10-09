#include "runtime/InputPreferences.hpp"
#include "ImGuiEditorLayer.hpp"
#include "render/RenderMath.hpp"
#include "input/EditorInputRouter.hpp"
#include "EditorTheme.hpp"
#include "EditorToolbar.hpp"
#include "resources/ResourceLocator.hpp"
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
    addPanel("outliner", "Scene Outliner", [this](PanelContext& context) { drawOutlinerPanel(context); });
    addPanel("inspector", "Inspector", [this](PanelContext& context) { drawInspectorPanel(context); });
    addPanel("assets", "Asset Browser", [this](PanelContext& context) { drawAssetBrowserPanel(context); });
    addPanel("capture", "Capture", [this](PanelContext& context) { drawCapturePanel(context); });
    addPanel("console", "Console", [this](PanelContext& context) { drawConsolePanel(context); });
    addPanel("build", "Build Game", [this](PanelContext& context) { drawBuildPanel(context); });
    addPanel("animation", "Animation Preview", [this](PanelContext& context) { drawAnimationPanel(context); });
    addPanel("gameplay-debug", "Gameplay Debug", [this](PanelContext& context) { drawGameplayDebugPanel(context); });
    addPanel("settings", "Settings", [this](PanelContext& context) { drawSettingsPanel(context); });
    addPanel("projects", "Projects", [this](PanelContext& context) { drawProjectBrowserPanel(context); });
    addPanel("environment", "Environment", [this](PanelContext& context) { drawEnvironmentPanel(context); });
    panels_ = registry.createAll();
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
    configDirectory_=EditorWorkspace::configDirectory();
    try{session_->setPathHistoryFile(configDirectory_/"path-history.json");}catch(const std::exception& error){session_->log(std::string("Path history: ")+error.what());}
    const bool restored=workspace_.load(configDirectory_);
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
    if(uiFrame_==0)if(const auto* path=std::getenv("AZURERENDER_EDITOR_UI_ACTIONS")) {
        std::ifstream file(std::filesystem::u8path(path));file>>uiActions_;
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

void ImGuiEditorLayer::newFrame() {
    if (!initialized_) {
        return;
    }
    session_->pollModel();
    session_->pollTasks();
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
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
        glfwSetInputMode(window_,GLFW_CURSOR,GLFW_CURSOR_NORMAL);
    }
    for (const std::unique_ptr<IEditorPanel>& panel : panels_) {
        if(!workspace_.visible(std::string(panel->id())))continue;
        const bool editable=std::string(panel->id())=="viewport" || std::string(panel->id())=="console" || std::string(panel->id())=="capture";
        ImGui::BeginDisabled((session_->playing() || session_->building()) && !editable && panel->id()!="build" && panel->id()!="settings");auto panelContext=session_->panelContext();panel->draw(panelContext);ImGui::EndDisabled();
    }
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
        event.playing=session_->playing();event.building=session_->building();
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
                widgets_["tab."+panel.id]={bar->BarRect.Min.x+tab.Offset,bar->BarRect.Min.y,tab.Width,bar->BarRect.GetHeight()};
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
        {"mouse",{io.MousePos.x,io.MousePos.y}},{"mouseDown",io.MouseDown[0]},
        {"activeId",ImGui::GetCurrentContext()->ActiveId},
        {"movingWindow",ImGui::GetCurrentContext()->MovingWindow?ImGui::GetCurrentContext()->MovingWindow->Name:""}});
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

void ImGuiEditorLayer::drawViewportPanel(PanelContext&) {
#ifndef IMGUI_HAS_DOCK
    setFallbackPanelRect(0.20F, 0.0F, 0.56F, 0.72F);
#endif
    if(!ImGui::Begin("Viewport###viewport",workspace_.open("viewport"))){cancelViewportGizmo();navigationButton_=-1;ImGui::End();return;}
    viewportFocused_ = ImGui::IsWindowFocused(
        ImGuiFocusedFlags_RootAndChildWindows);
    if (!viewportTextures_.empty()) {
        const bool compact=ImGui::GetIO().DisplaySize.y/dpi_<720 || session_->settings().get("editor.compact").get<bool>();
        if(compact){ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{8*dpi_,0});ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{8*dpi_,2*dpi_});}
        if(ImGui::Button("Focus Selected")) {
            session_->edit("viewport.frame-selection");
        }observeWidget("focus");ImGui::SameLine();
        if(ImGui::Button("Transform"))ImGui::OpenPopup("Transform options");observeWidget("gizmo.options");
        if(ImGui::BeginPopup("Transform options")) {
            ImGui::BeginDisabled(session_->gizmo().active());
            for(const auto* space:{"world","local"}) {
                if(ImGui::Selectable(space,(context_->gizmoSpace()==EditorContext::GizmoSpace::World)==(std::string(space)=="world")))session_->edit("viewport.gizmo-options",{{"space",space}});
                observeWidget(std::string("gizmo.space.")+space);
            }
            ImGui::Separator();
            for(const auto* pivot:{"active","bounds"}) {
                if(ImGui::Selectable(pivot,(context_->gizmoPivot()==EditorContext::GizmoPivot::Active)==(std::string(pivot)=="active")))session_->edit("viewport.gizmo-options",{{"pivot",pivot}});
                observeWidget(std::string("gizmo.pivot.")+pivot);
            }
            bool snap=session_->settings().get("editor.gizmo.snap").get<bool>();
            if(ImGui::Checkbox("Snap (Ctrl)",&snap))session_->edit("settings.set",{{"name","editor.gizmo.snap"},{"value",snap}});observeWidget("gizmo.snap");
            for(const auto& entry:std::vector<std::pair<const char*,const char*>>{{"Move step","editor.gizmo.moveStep"},{"Rotate degrees","editor.gizmo.rotateStep"},{"Scale step","editor.gizmo.scaleStep"}}) {
                float step=session_->settings().get(entry.second).get<float>();
                if(ImGui::InputFloat(entry.first,&step))session_->edit("settings.set",{{"name",entry.second},{"value",step}});
            }
            ImGui::EndDisabled();ImGui::EndPopup();
        }
        if(compact)ImGui::PopStyleVar(2);
        const ImVec2 available = ImGui::GetContentRegionAvail();
        const ImVec2 framebufferScale = ImGui::GetIO().DisplayFramebufferScale;
        const std::uint32_t desiredWidth = static_cast<std::uint32_t>(
            std::max(std::floor(available.x * framebufferScale.x), 64.0F));
        const std::uint32_t desiredHeight = static_cast<std::uint32_t>(
            std::max(std::floor(available.y * framebufferScale.y), 64.0F));
        if (desiredWidth == resizeCandidateWidth_
            && desiredHeight == resizeCandidateHeight_) {
            resizeStableFrames_ = std::min(resizeStableFrames_ + 1, 60U);
        } else {
            resizeCandidateWidth_ = desiredWidth;
            resizeCandidateHeight_ = desiredHeight;
            resizeStableFrames_ = 0;
        }
        if (resizeStableFrames_ >= 4
            && (desiredWidth != viewportWidth_
                || desiredHeight != viewportHeight_)
            && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            viewportResizePending_ = true;
        }
        const float aspect = static_cast<float>(viewportWidth_)
            / static_cast<float>(viewportHeight_);
        ImVec2 imageSize{available.x, available.x / aspect};
        if (imageSize.y > available.y) {
            imageSize.y = available.y;
            imageSize.x = available.y * aspect;
        }
        const ImVec2 cursor = ImGui::GetCursorPos();
        ImGui::SetCursorPos({
            cursor.x + std::max((available.x - imageSize.x) * 0.5F, 0.0F),
            cursor.y + std::max((available.y - imageSize.y) * 0.5F, 0.0F),
        });
        ImGui::Image(
            reinterpret_cast<ImTextureID>(
                viewportTextures_[viewportImageIndex_]),
            imageSize);
        if(!session_->playing() && !session_->building() && ImGui::BeginDragDropTarget()){
            if(const auto* payload=ImGui::AcceptDragDropPayload("AZURE_RESOURCE")){
                const auto minimum=ImGui::GetItemRectMin();const auto mouse=ImGui::GetMousePos();
                const auto direction=internal::pickRayDirection(cameraPosition_,cameraTarget_,(mouse.x-minimum.x)/imageSize.x,(mouse.y-minimum.y)/imageSize.y,imageSize.x/imageSize.y);
                session_->edit("asset.place",{{"asset",static_cast<const char*>(payload->Data)},{"origin",cameraPosition_},{"direction",direction}});
            }
            ImGui::EndDragDropTarget();
        }
        // Draw the viewport gizmo handles (if any selected primitive has a
        // valid screen projection). We compute endpoints here so the click
        // and drag logic can hit-test against them.
        const ImVec2 itemMin = ImGui::GetItemRectMin();
        imageRect_={itemMin.x,itemMin.y,imageSize.x,imageSize.y};
        widgets_["viewport.image"]=imageRect_;
        for(auto item=widgets_.begin();item!=widgets_.end();)if(item.key().rfind("pick.",0)==0)item=widgets_.erase(item);else ++item;
        if(context_->isProject()) {
            for(const auto& target:context_->pickTargets()) {
                const auto projected=context_->projectDebugPoint(target.second);
                if(projected)widgets_["pick."+target.first]={itemMin.x+(*projected)[0]*imageSize.x-1,itemMin.y+(*projected)[1]*imageSize.y-1,2,2};
            }
        }
        if(session_->debugOverlay){
            auto* draw=ImGui::GetWindowDrawList();draw->PushClipRect(itemMin,{itemMin.x+imageSize.x,itemMin.y+imageSize.y},true);
            auto line=[&](const DebugLine& edge,ImU32 color){auto a=context_->projectDebugPoint(edge.from),b=context_->projectDebugPoint(edge.to);
                if(a&&b){draw->AddLine({itemMin.x+(*a)[0]*imageSize.x,itemMin.y+(*a)[1]*imageSize.y},{itemMin.x+(*b)[0]*imageSize.x,itemMin.y+(*b)[1]*imageSize.y},color,1.5F);++debugLineCount_;}};
            const auto scene=session_->viewScene();auto* runtime=session_->runtime();const auto registry=reflection::makeRuntimeRegistry();
            for(const auto& node:scene.nodes){
                ecs::TransformComponent transform{node.translation,node.rotation,node.scale};
                game::RigidBody body;game::Character character;bool hasBody=false,hasCharacter=false;
                if(runtime){const auto entity=runtime->entity(node.id);if(const auto* value=runtime->world().tryGet<game::RigidBody>(entity)){body=*value;hasBody=true;}if(const auto* value=runtime->world().tryGet<game::Character>(entity)){character=*value;hasCharacter=true;}}
                else{auto data=context_->componentData(node.id,"azure.rigid-body");if(!data.is_null()){registry.decode("azure.rigid-body",&body,{{"type","azure.rigid-body"},{"version",1},{"data",data}});hasBody=true;}
                    data=context_->componentData(node.id,"azure.character");if(!data.is_null()){registry.decode("azure.character",&character,{{"type","azure.character"},{"version",registry.type("azure.character").version},{"data",data}});hasCharacter=true;}}
                if(hasBody)for(const auto& edge:debugBox(transform,body))line(edge,body.trigger?IM_COL32(255,210,60,220):IM_COL32(70,220,120,220));
                if(hasCharacter)for(const auto& edge:debugCapsule(transform,character))line(edge,IM_COL32(80,180,255,230));
            }
            if(auto* game=session_->game();game && game->hasCamera())line({game->camera().target(),game->camera().position()},IM_COL32(255,80,170,255));
            draw->PopClipRect();
        }
        if(session_->playing() && gameUi_ && imageSize.x>0 && imageSize.y>0){
            const auto mouse=ImGui::GetMousePos();const bool consumed=gameUi_->pointer(static_cast<int>((mouse.x-itemMin.x)*viewportWidth_/imageSize.x),static_cast<int>((mouse.y-itemMin.y)*viewportHeight_/imageSize.y),ImGui::IsItemHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Left));
            if(consumed||gameUi_->wantsKeyboard())viewportAcceptsShortcuts_=false;
            if(viewportFocused_){for(auto character:ImGui::GetIO().InputQueueCharacters)gameUi_->character(character);}
        }
        const bool hovered=ImGui::IsItemHovered();
        const bool gizmoOver=drawViewportGizmo(itemMin,imageSize);
        const auto& navIo=ImGui::GetIO();
        const bool navigationAllowed=!session_->playing() && !session_->building() && !navIo.WantTextInput
            && !ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId|ImGuiPopupFlags_AnyPopupLevel)
            && !navIo.AppFocusLost && !viewportGizmoDragActive_;
        if(!navigationAllowed)navigationButton_=-1;
        if(navigationAllowed && hovered && navigationButton_<0) {
            if(ImGui::IsMouseClicked(ImGuiMouseButton_Right))navigationButton_=ImGuiMouseButton_Right;
            else if(ImGui::IsMouseClicked(ImGuiMouseButton_Middle))navigationButton_=ImGuiMouseButton_Middle;
            else if(navIo.KeyAlt && ImGui::IsMouseClicked(ImGuiMouseButton_Left))navigationButton_=ImGuiMouseButton_Left;
        }
        if(navigationButton_>=0 && (!ImGui::IsMouseDown(navigationButton_) || ImGui::IsKeyPressed(ImGuiKey_Escape,false)))navigationButton_=-1;
        if (hovered || navigationButton_>=0 || viewportGizmoDragActive_) {
            const ImGuiIO& io = ImGui::GetIO();
            if(navigationAllowed && hovered)viewportInput_.zoomSteps += io.MouseWheel;
            const float sx=session_->settings().get("editor.camera.invertX").get<bool>()?-1.F:1.F;
            const float sy=session_->settings().get("editor.camera.invertY").get<bool>()?-1.F:1.F;
            const auto sensitivity=cameraSensitivity(session_->settings());
            const auto mouseDelta=navigationButton_>=0 && !ImGui::IsMouseClicked(navigationButton_)?io.MouseDelta:ImVec2{0,0};
            if(navigationButton_==ImGuiMouseButton_Right) {
                viewportInput_.lookDeltaX+=mouseDelta.x*sensitivity*sx;
                viewportInput_.lookDeltaY+=mouseDelta.y*sensitivity*sy;
                viewportInput_.flyForward=static_cast<float>(ImGui::IsKeyDown(ImGuiKey_W))-static_cast<float>(ImGui::IsKeyDown(ImGuiKey_S));
                viewportInput_.flyRight=static_cast<float>(ImGui::IsKeyDown(ImGuiKey_D))-static_cast<float>(ImGui::IsKeyDown(ImGuiKey_A));
                viewportInput_.flyUp=static_cast<float>(ImGui::IsKeyDown(ImGuiKey_E))-static_cast<float>(ImGui::IsKeyDown(ImGuiKey_Q));
                viewportInput_.flySpeed=session_->settings().get("editor.camera.flySpeed").get<float>()*(io.KeyShift?session_->settings().get("editor.camera.boost").get<float>():1.F);
                viewportInput_.deltaSeconds=io.DeltaTime;
            }else if(navigationButton_==ImGuiMouseButton_Left) {
                const auto orbitSensitivity=session_->settings().get("editor.camera.orbitSensitivity").get<float>();
                viewportInput_.orbitDeltaX+=mouseDelta.x*orbitSensitivity*sx;
                viewportInput_.orbitDeltaY+=mouseDelta.y*orbitSensitivity*sy;
            }else if(navigationButton_==ImGuiMouseButton_Middle){viewportInput_.panDeltaX+=mouseDelta.x;viewportInput_.panDeltaY+=mouseDelta.y;}

            const bool pickThisClick=!gizmoOver && !viewportGizmoDragActive_;
            if (!session_->playing() && !session_->building() && pickThisClick
                && navigationButton_<0 && hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                const ImVec2 mousePosition = io.MousePos;
                if (imageSize.x > 0.0F && imageSize.y > 0.0F) {
                    viewportInput_.pickX = std::clamp(
                        (mousePosition.x - itemMin.x) / imageSize.x,
                        0.0F,
                        1.0F);
                    viewportInput_.pickY = std::clamp(
                        (mousePosition.y - itemMin.y) / imageSize.y,
                        0.0F,
                        1.0F);
                    viewportInput_.pickRequested = true;
                    viewportInput_.pickAdditive=io.KeyCtrl;
                }
            }

        }
    }
    viewportAcceptsShortcuts_ = (viewportFocused_ || navigationButton_>=0)
        && !ImGui::GetIO().WantTextInput
        && !ImGui::IsAnyItemActive();
    ImGui::End();
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
void ImGuiEditorLayer::newFrame() {}
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
