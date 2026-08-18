#include "ImGuiEditorLayer.hpp"
#include "diagnostics/RuntimeDiagnostics.hpp"
#include "extensions/ExtensionRegistry.hpp"

#ifdef AZURERENDER_HAS_IMGUI
#include <imgui.h>
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
#include <stdexcept>
#include <utility>
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
        std::function<void()> draw)
        : id_(id), title_(title), draw_(std::move(draw)) {}

    [[nodiscard]] std::string_view id() const noexcept override { return id_; }
    [[nodiscard]] std::string_view title() const noexcept override {
        return title_;
    }
    void draw(EditorContext&) override { draw_(); }

private:
    std::string_view id_;
    std::string_view title_;
    std::function<void()> draw_;
};

}  // namespace

ImGuiEditorLayer::ImGuiEditorLayer(std::shared_ptr<EditorSession> session)
    : session_(std::move(session)) {
    if (session_ == nullptr) {
        throw std::invalid_argument("ImGui editor requires an editor session");
    }
    context_ = &session_->context();
    EditorPanelRegistry registry;
    const auto addPanel = [&registry](
                              const char* id,
                              const char* title,
                              std::function<void()> draw) {
        registry.registerFactory(
            {std::string("panel.") + id, 1, {"editor.panel"}, {}},
            [id, title, draw = std::move(draw)] {
                return std::make_unique<CallbackEditorPanel>(
                    id, title, draw);
            });
    };
    addPanel("viewport", "Viewport", [this] { drawViewportPanel(); });
    addPanel("outliner", "Scene Outliner", [this] { drawOutlinerPanel(); });
    addPanel("inspector", "Inspector", [this] { drawInspectorPanel(); });
    addPanel("assets", "Asset Browser", [this] { drawAssetBrowserPanel(); });
    addPanel("capture", "Capture", [this] { drawCapturePanel(); });
    addPanel("console", "Console", [this] { drawConsolePanel(); });
    panels_ = registry.createAll();
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
    const std::uint32_t imageCount) {
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
    io.IniFilename = "azurerender-editor.ini";
    ImGui::StyleColorsDark();
    ImGui::GetStyle().Colors[ImGuiCol_WindowBg].w = 1.0F;

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
    for (const VkDescriptorSet texture : viewportTextures_) {
        ImGui_ImplVulkan_RemoveTexture(texture);
    }
    viewportTextures_.clear();
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
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiEditorLayer::drawPanels() {
    if (!initialized_) {
        return;
    }
#ifdef IMGUI_HAS_DOCK
    const ImGuiID dockspace = ImGui::DockSpaceOverViewport(
        0,
        ImGui::GetMainViewport(),
        ImGuiDockNodeFlags_PassthruCentralNode);
    const bool resetLayout = session_->consumeLayoutResetRequest();
    if (resetLayout) {
        ImGui::DockBuilderRemoveNode(dockspace);
        dockingLayoutInitialized_ = false;
    }
    if (!dockingLayoutInitialized_) {
        ImGuiDockNode* node = ImGui::DockBuilderGetNode(dockspace);
        if (resetLayout || (node != nullptr && node->IsEmpty())) {
            ImGui::DockBuilderRemoveNode(dockspace);
            ImGui::DockBuilderAddNode(
                dockspace,
                ImGuiDockNodeFlags_DockSpace
                    | ImGuiDockNodeFlags_PassthruCentralNode);
            ImGui::DockBuilderSetNodeSize(
                dockspace, ImGui::GetMainViewport()->Size);
            ImGuiID center = dockspace;
            const ImGuiID left = ImGui::DockBuilderSplitNode(
                center, ImGuiDir_Left, 0.20F, nullptr, &center);
            const ImGuiID right = ImGui::DockBuilderSplitNode(
                center, ImGuiDir_Right, 0.24F, nullptr, &center);
            const ImGuiID bottom = ImGui::DockBuilderSplitNode(
                center, ImGuiDir_Down, 0.28F, nullptr, &center);
            ImGui::DockBuilderDockWindow("Scene Outliner", left);
            ImGui::DockBuilderDockWindow("Inspector", right);
            ImGui::DockBuilderDockWindow("Asset Browser", bottom);
            ImGui::DockBuilderDockWindow("Capture", bottom);
            ImGui::DockBuilderDockWindow("Console", bottom);
            ImGui::DockBuilderDockWindow("Viewport", center);
            ImGui::DockBuilderFinish(dockspace);
        }
        dockingLayoutInitialized_ = true;
    }
#endif
    const ImGuiIO& io = ImGui::GetIO();
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false)) {
        static_cast<void>(session_->execute(EditorCommand::Save));
    }
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
        static_cast<void>(session_->execute(EditorCommand::Undo));
    }
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y, false)) {
        static_cast<void>(session_->execute(EditorCommand::Redo));
    }
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Save", "Ctrl+S")) {
                static_cast<void>(session_->execute(EditorCommand::Save));
            }
            if (ImGui::MenuItem("Reload")) {
                static_cast<void>(session_->execute(EditorCommand::Reload));
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo", "Ctrl+Z", false, context_->canUndo())) {
                static_cast<void>(session_->execute(EditorCommand::Undo));
            }
            if (ImGui::MenuItem("Redo", "Ctrl+Y", false, context_->canRedo())) {
                static_cast<void>(session_->execute(EditorCommand::Redo));
            }
            if (ImGui::MenuItem("Reload Assets")) {
                static_cast<void>(session_->execute(EditorCommand::ReloadAssets));
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            if (ImGui::MenuItem("Reset Layout")) {
                static_cast<void>(
                    session_->execute(EditorCommand::ResetLayout));
            }
            ImGui::EndMenu();
        }
        if (context_->dirty()) {
            ImGui::TextUnformatted("Unsaved changes");
        }
        if (!session_->lastError().empty()) {
            ImGui::TextColored(
                ImVec4(0.95F, 0.35F, 0.30F, 1.0F),
                "%s",
                session_->lastError().c_str());
        }
        ImGui::EndMainMenuBar();
    }
    for (const std::unique_ptr<IEditorPanel>& panel : panels_) {
        panel->draw(*context_);
    }
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

void ImGuiEditorLayer::drawViewportPanel() {
#ifndef IMGUI_HAS_DOCK
    setFallbackPanelRect(0.20F, 0.0F, 0.56F, 0.72F);
#endif
    ImGui::Begin("Viewport");
    viewportFocused_ = ImGui::IsWindowFocused(
        ImGuiFocusedFlags_RootAndChildWindows);
    if (!viewportTextures_.empty()) {
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
        // Draw the viewport gizmo handles (if any selected primitive has a
        // valid screen projection). We compute endpoints here so the click
        // and drag logic can hit-test against them.
        const auto& gizmoScreen = context_->gizmoScreen();
        const ImVec2 itemMin = ImGui::GetItemRectMin();
        ImVec2 gizmoCenter{0.0F, 0.0F};
        ImVec2 gizmoAxisEnds[3] = {{0.0F, 0.0F}, {0.0F, 0.0F}, {0.0F, 0.0F}};
        bool gizmoDrawn = false;
        if (gizmoScreen.valid && imageSize.x > 0.0F && imageSize.y > 0.0F) {
            gizmoCenter = ImVec2(
                itemMin.x + gizmoScreen.centerX * imageSize.x,
                itemMin.y + gizmoScreen.centerY * imageSize.y);
            const float axes[3][2] = {
                {gizmoScreen.axisXScreenX, gizmoScreen.axisXScreenY},
                {gizmoScreen.axisYScreenX, gizmoScreen.axisYScreenY},
                {gizmoScreen.axisZScreenX, gizmoScreen.axisZScreenY},
            };
            const ImU32 colors[3] = {
                IM_COL32(230, 80, 80, 255),
                IM_COL32(80, 220, 110, 255),
                IM_COL32(90, 140, 230, 255),
            };
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            for (int axis = 0; axis < 3; ++axis) {
                gizmoAxisEnds[axis] = ImVec2(
                    gizmoCenter.x + axes[axis][0] * 40.0F,
                    gizmoCenter.y + axes[axis][1] * 40.0F);
                drawList->AddLine(
                    gizmoCenter, gizmoAxisEnds[axis], colors[axis], 3.0F);
                drawList->AddCircleFilled(
                    gizmoAxisEnds[axis], 5.0F, colors[axis]);
            }
            gizmoDrawn = true;
        }
        if (ImGui::IsItemHovered()) {
            const ImGuiIO& io = ImGui::GetIO();
            viewportInput_.zoomSteps += io.MouseWheel;
            if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
                viewportInput_.orbitDeltaX += io.MouseDelta.x;
                viewportInput_.orbitDeltaY += io.MouseDelta.y;
            }
            if (ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
                viewportInput_.panDeltaX += io.MouseDelta.x;
                viewportInput_.panDeltaY += io.MouseDelta.y;
            }
            bool pickThisClick = true;
            if (gizmoDrawn
                && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                const ImVec2 mousePosition = io.MousePos;
                float bestDistance = 12.0F;
                std::int32_t bestAxis = -1;
                for (int axis = 0; axis < 3; ++axis) {
                    const float distance = std::hypot(
                        mousePosition.x - gizmoAxisEnds[axis].x,
                        mousePosition.y - gizmoAxisEnds[axis].y);
                    if (distance < bestDistance) {
                        bestDistance = distance;
                        bestAxis = axis;
                    }
                }
                if (bestAxis >= 0) {
                    gizmoDragAxis_ = bestAxis;
                    gizmoDragStartMouse_ = mousePosition;
                    gizmoDragStartTranslation_ = context_->gizmoTranslation();
                    viewportGizmoDragActive_ = true;
                    pickThisClick = false;
                }
            }
            if (pickThisClick
                && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
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
                }
            }
            if (viewportGizmoDragActive_
                && gizmoDragAxis_ >= 0
                && gizmoScreen.valid
                && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                const float axes[3][2] = {
                    {gizmoScreen.axisXScreenX, gizmoScreen.axisXScreenY},
                    {gizmoScreen.axisYScreenX, gizmoScreen.axisYScreenY},
                    {gizmoScreen.axisZScreenX, gizmoScreen.axisZScreenY},
                };
                const float projection =
                    io.MouseDelta.x * axes[gizmoDragAxis_][0]
                    + io.MouseDelta.y * axes[gizmoDragAxis_][1];
                const float worldDelta = projection * gizmoScreen.pixelToWorld;
                std::array<float, 3> translation =
                    gizmoDragStartTranslation_;
                translation[gizmoDragAxis_] += worldDelta;
                context_->setGizmoTranslation(translation);
            }
            if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                viewportGizmoDragActive_ = false;
                gizmoDragAxis_ = -1;
            }
        }
    }
    viewportAcceptsShortcuts_ = viewportFocused_
        && !ImGui::GetIO().WantTextInput
        && !ImGui::IsAnyItemActive();
    ImGui::End();
}

void ImGuiEditorLayer::drawOutlinerPanel() {
#ifndef IMGUI_HAS_DOCK
    setFallbackPanelRect(0.0F, 0.0F, 0.20F, 0.72F);
#endif
    ImGui::Begin("Scene Outliner");
    const auto& nodes = context_->scene().nodes;
    if (ImGui::Button("Add Child")) {
        if (!nodes.empty()) {
            try {
                context_->addChildNode(context_->selectedNodeIndex());
            } catch (const std::exception& exception) {
                context_->log(std::string("ERROR: ") + exception.what());
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete") && !nodes.empty()) {
        context_->removeNode(context_->selectedNodeIndex());
    }
    ImGui::Separator();
    if (nodes.empty()) {
        ImGui::TextUnformatted("(empty scene)");
        ImGui::End();
        return;
    }
    // Recursive tree draw: children are nodes whose parentId equals the
    // current node id. Use an index-based recursion to avoid iterator
    // invalidation while nodes stay stable during a frame.
    const auto drawNode = [&](const auto& self,
                              const std::string& parentId,
                              const int depth) -> void {
        for (std::size_t index = 0; index < nodes.size(); ++index) {
            if (nodes[index].parentId != parentId) {
                continue;
            }
            const bool selected = index == context_->selectedNodeIndex();
            bool hasChildren = false;
            for (const SceneNode& candidate : nodes) {
                if (candidate.parentId == nodes[index].id) {
                    hasChildren = true;
                    break;
                }
            }
            ImGui::PushID(static_cast<int>(index));
            bool open = false;
            if (hasChildren) {
                const bool clicked = ImGui::TreeNodeEx(
                    nodes[index].name.c_str(),
                    ImGuiTreeNodeFlags_OpenOnArrow
                        | ImGuiTreeNodeFlags_SpanAvailWidth
                        | (selected ? ImGuiTreeNodeFlags_Selected : 0));
                open = clicked;
            } else {
                ImGui::Selectable(
                    nodes[index].name.c_str(),
                    selected,
                    ImGuiSelectableFlags_SpanAvailWidth);
            }
            if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
                context_->selectNode(index);
            }
            if (open) {
                self(self, nodes[index].id, depth + 1);
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
    };
    drawNode(drawNode, "", 0);
    ImGui::End();
}

void ImGuiEditorLayer::drawInspectorPanel() {
#ifndef IMGUI_HAS_DOCK
    setFallbackPanelRect(0.76F, 0.0F, 0.24F, 0.72F);
#endif
    ImGui::Begin("Inspector");
    if (const SceneNode* node = context_->selectedNode(); node != nullptr) {
        ImGui::Text("Node: %s", node->name.c_str());
        ImGui::Text("Id: %s", node->id.c_str());
        bool visible = node->visible;
        if (ImGui::Checkbox("Visible", &visible)) {
            context_->setSelectedNodeVisible(visible);
        }
        ImGui::TextUnformatted("Name");
        std::array<char, 128> nameBuffer{};
        const std::size_t copyLength = std::min(
            node->name.size(), nameBuffer.size() - 1);
        std::memcpy(
            nameBuffer.data(), node->name.data(), copyLength);
        if (ImGui::InputText("##name", nameBuffer.data(), nameBuffer.size())) {
            context_->setSelectedNodeName(nameBuffer.data());
        }
        ImGui::TextUnformatted("Prefab Source");
        std::array<char, 256> prefabBuffer{};
        std::memcpy(prefabBuffer.data(), node->prefabSource.data(),
            std::min(node->prefabSource.size(), prefabBuffer.size() - 1));
        if (ImGui::InputText("##prefab", prefabBuffer.data(), prefabBuffer.size())) {
            context_->setSelectedNodePrefab(prefabBuffer.data());
        }
        ImGui::TextUnformatted("Instance Of");
        std::array<char, 128> instanceBuffer{};
        std::memcpy(instanceBuffer.data(), node->instanceOf.data(),
            std::min(node->instanceOf.size(), instanceBuffer.size() - 1));
        if (ImGui::InputText("##instance", instanceBuffer.data(), instanceBuffer.size())) {
            context_->setSelectedNodeInstance(instanceBuffer.data());
        }
    }
    ImGui::Separator();
    ImGui::Text("Transform Gizmo");
    std::array<float, 3> translation = context_->gizmoTranslation();
    if (ImGui::DragFloat3("Translate", translation.data(), 0.01F)) {
        context_->setGizmoTranslation(translation);
    }
    std::array<float, 3> rotation = context_->gizmoRotation();
    if (ImGui::DragFloat3("Rotate (deg)", rotation.data(), 0.5F)) {
        context_->setGizmoRotation(rotation);
    }
    std::array<float, 3> scale = context_->gizmoScale();
    if (ImGui::DragFloat3("Scale", scale.data(), 0.01F, 0.01F, 100.0F)) {
        context_->setGizmoScale(scale);
    }
    RenderSettings& settings = context_->renderSettings();
    if (ImGui::BeginCombo(
            "Showcase Look",
            std::string(showcasePresetName(settings.showcasePreset)).c_str())) {
        for (std::uint32_t preset = 0; preset < 5; ++preset) {
            const bool selected = settings.showcasePreset == preset;
            const std::string name(showcasePresetName(preset));
            if (ImGui::Selectable(name.c_str(), selected)) {
                context_->beginEdit();
                applyShowcasePresetLook(settings, preset);
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    bool background = settings.characterPresentation.backgroundEnabled;
    if (ImGui::Checkbox("Background", &background)) {
        context_->beginEdit();
        settings.characterPresentation.backgroundEnabled = background;
    }
    bool platform = settings.characterPresentation.platformEnabled;
    if (ImGui::Checkbox("Showcase Platform", &platform)) {
        context_->beginEdit();
        settings.characterPresentation.platformEnabled = platform;
    }
    bool faceSdf = settings.faceSdf.enabled;
    if (ImGui::Checkbox("Face SDF", &faceSdf)) {
        context_->beginEdit();
        settings.faceSdf.enabled = faceSdf;
    }
    float threshold = settings.faceSdf.threshold;
    if (ImGui::SliderFloat("Face SDF Threshold", &threshold, 0.0F, 1.0F)) {
        context_->beginEdit();
        settings.faceSdf.threshold = threshold;
    }
    float softness = settings.faceSdf.softness;
    if (ImGui::SliderFloat("Face SDF Softness", &softness, 0.001F, 0.5F)) {
        context_->beginEdit();
        settings.faceSdf.softness = softness;
    }
    float outline = settings.outline.strength;
    if (ImGui::SliderFloat("Outline", &outline, 0.0F, 2.0F)) {
        context_->beginEdit();
        settings.outline.strength = outline;
    }
    float exposure = settings.grade.exposureEv;
    if (ImGui::SliderFloat("Exposure EV", &exposure, -8.0F, 8.0F)) {
        context_->beginEdit();
        settings.grade.exposureEv = exposure;
    }
    ImGui::End();
}

void ImGuiEditorLayer::drawAssetBrowserPanel() {
#ifndef IMGUI_HAS_DOCK
    setFallbackPanelRect(0.0F, 0.72F, 0.50F, 0.28F);
#endif
    ImGui::Begin("Asset Browser");
    if (ImGui::Button("Reload Assets")) {
        static_cast<void>(session_->execute(EditorCommand::ReloadAssets));
    }
    if (ImGui::BeginTable(
            "assets", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Id");
        ImGui::TableSetupColumn("Path");
        ImGui::TableSetupColumn("State");
        ImGui::TableSetupColumn("Users");
        ImGui::TableHeadersRow();
        for (const EditorContext::ResourceStatus& resource
             : context_->resourceStatuses()) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(resource.id.c_str());
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(resource.path.generic_string().c_str());
            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(resource.exists ? "Ready" : "Missing");
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%zu", resource.dependentNodeCount);
        }
        ImGui::EndTable();
    }
    ImGui::End();
}

void ImGuiEditorLayer::drawCapturePanel() {
    ImGui::Begin("Capture");
    std::array<char, 128> label{};
    const std::string& current = session_->captureLabel();
    std::memcpy(label.data(), current.data(),
        std::min(current.size(), label.size() - 1));
    if (ImGui::InputText("Label", label.data(), label.size())) {
        session_->setCaptureLabel(label.data());
    }
    if (ImGui::Button("Capture Viewport")) {
        static_cast<void>(session_->execute(EditorCommand::Capture));
    }
    ImGui::SameLine();
    ImGui::TextUnformatted("PNG + semantic label");
    ImGui::End();
}

void ImGuiEditorLayer::drawConsolePanel() {
#ifndef IMGUI_HAS_DOCK
    setFallbackPanelRect(0.50F, 0.72F, 0.50F, 0.28F);
#endif
    ImGui::Begin("Console");
    for (const std::string& message :
         azurerender::RuntimeDiagnostics::instance().messages()) {
        ImGui::TextUnformatted(message.c_str());
    }
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
    VkQueue, VkRenderPass, std::uint32_t) {}

void ImGuiEditorLayer::shutdownVulkan() {}
void ImGuiEditorLayer::newFrame() {}
void ImGuiEditorLayer::drawPanels() {}
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
