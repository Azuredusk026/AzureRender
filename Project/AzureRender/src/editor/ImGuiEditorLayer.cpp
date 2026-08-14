#include "ImGuiEditorLayer.hpp"

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

ImGuiEditorLayer::ImGuiEditorLayer(std::shared_ptr<EditorContext> context)
    : context_(std::move(context)) {
    if (context_ == nullptr) {
        throw std::invalid_argument("ImGui editor requires an editor context");
    }
    panels_.push_back(std::make_unique<CallbackEditorPanel>(
        "viewport", "Viewport", [this] { drawViewportPanel(); }));
    panels_.push_back(std::make_unique<CallbackEditorPanel>(
        "outliner", "Scene Outliner", [this] { drawOutlinerPanel(); }));
    panels_.push_back(std::make_unique<CallbackEditorPanel>(
        "inspector", "Inspector", [this] { drawInspectorPanel(); }));
    panels_.push_back(std::make_unique<CallbackEditorPanel>(
        "assets", "Asset Browser", [this] { drawAssetBrowserPanel(); }));
    panels_.push_back(std::make_unique<CallbackEditorPanel>(
        "console", "Console", [this] { drawConsolePanel(); }));
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
    initInfo.Instance = instance;
    initInfo.PhysicalDevice = physicalDevice;
    initInfo.Device = device;
    initInfo.QueueFamily = queueFamily;
    initInfo.Queue = queue;
    initInfo.DescriptorPool = descriptorPool_;
    initInfo.MinImageCount = 2;
    initInfo.ImageCount = imageCount;
    initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    if (!ImGui_ImplVulkan_Init(&initInfo, renderPass)) {
        vkDestroyDescriptorPool(device, descriptorPool_, nullptr);
        descriptorPool_ = VK_NULL_HANDLE;
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        throw std::runtime_error("ImGui Vulkan backend initialization failed");
    }
    if (!ImGui_ImplVulkan_CreateFontsTexture()) {
        ImGui_ImplVulkan_Shutdown();
        vkDestroyDescriptorPool(device, descriptorPool_, nullptr);
        descriptorPool_ = VK_NULL_HANDLE;
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        throw std::runtime_error("ImGui font texture creation failed");
    }
    initialized_ = true;
}

void ImGuiEditorLayer::shutdownVulkan() {
    if (!initialized_) {
        return;
    }
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
        ImGui::GetMainViewport(),
        ImGuiDockNodeFlags_PassthruCentralNode);
    if (!dockingLayoutInitialized_) {
        ImGuiDockNode* node = ImGui::DockBuilderGetNode(dockspace);
        if (node != nullptr && node->IsEmpty()) {
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
            ImGui::DockBuilderDockWindow("Console", bottom);
            ImGui::DockBuilderDockWindow("Viewport", center);
            ImGui::DockBuilderFinish(dockspace);
        }
        dockingLayoutInitialized_ = true;
    }
#endif
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

bool ImGuiEditorLayer::wantsKeyboardCapture() const noexcept {
    return initialized_ && ImGui::GetIO().WantCaptureKeyboard;
}

void ImGuiEditorLayer::drawViewportPanel() {
#ifndef IMGUI_HAS_DOCK
    setFallbackPanelRect(0.20F, 0.0F, 0.56F, 0.72F);
#endif
    ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoBackground);
    ImGui::TextUnformatted("Live Vulkan Renderer");
    ImGui::Text("Scene: %s", context_->scene().sceneId.c_str());
    ImGui::End();
}

void ImGuiEditorLayer::drawOutlinerPanel() {
#ifndef IMGUI_HAS_DOCK
    setFallbackPanelRect(0.0F, 0.0F, 0.20F, 0.72F);
#endif
    ImGui::Begin("Scene Outliner");
    const auto& nodes = context_->scene().nodes;
    for (std::size_t index = 0; index < nodes.size(); ++index) {
        const bool selected = index == context_->selectedNodeIndex();
        if (ImGui::Selectable(nodes[index].name.c_str(), selected)) {
            context_->selectNode(index);
        }
    }
    ImGui::End();
}

void ImGuiEditorLayer::drawInspectorPanel() {
#ifndef IMGUI_HAS_DOCK
    setFallbackPanelRect(0.76F, 0.0F, 0.24F, 0.72F);
#endif
    ImGui::Begin("Inspector");
    if (const SceneNode* node = context_->selectedNode(); node != nullptr) {
        ImGui::Text("Node: %s", node->name.c_str());
        bool visible = node->visible;
        if (ImGui::Checkbox("Visible", &visible)) {
            context_->scene().nodes[context_->selectedNodeIndex()].visible = visible;
            context_->markDirty();
        }
    }
    RenderSettings& settings = context_->renderSettings();
    float outline = settings.outline.strength;
    if (ImGui::SliderFloat("Outline", &outline, 0.0F, 2.0F)) {
        settings.outline.strength = outline;
        context_->markDirty();
    }
    float exposure = settings.grade.exposureEv;
    if (ImGui::SliderFloat("Exposure EV", &exposure, -8.0F, 8.0F)) {
        settings.grade.exposureEv = exposure;
        context_->markDirty();
    }
    ImGui::End();
}

void ImGuiEditorLayer::drawAssetBrowserPanel() {
#ifndef IMGUI_HAS_DOCK
    setFallbackPanelRect(0.0F, 0.72F, 0.50F, 0.28F);
#endif
    ImGui::Begin("Asset Browser");
    for (const SceneResource& resource : context_->scene().resources) {
        ImGui::BulletText("%s: %s", resource.type.c_str(),
                          resource.path.generic_string().c_str());
    }
    ImGui::End();
}

void ImGuiEditorLayer::drawConsolePanel() {
#ifndef IMGUI_HAS_DOCK
    setFallbackPanelRect(0.50F, 0.72F, 0.50F, 0.28F);
#endif
    ImGui::Begin("Console");
    for (const std::string& message : context_->consoleMessages()) {
        ImGui::TextUnformatted(message.c_str());
    }
    ImGui::End();
}

}  // namespace azurerender
#else

ImGuiEditorLayer::ImGuiEditorLayer(std::shared_ptr<EditorContext> context)
    : context_(std::move(context)) {}

ImGuiEditorLayer::~ImGuiEditorLayer() = default;

void ImGuiEditorLayer::initialize(
    GLFWwindow*, VkInstance, VkPhysicalDevice, VkDevice, std::uint32_t,
    VkQueue, VkRenderPass, std::uint32_t) {}

void ImGuiEditorLayer::shutdownVulkan() {}
void ImGuiEditorLayer::newFrame() {}
void ImGuiEditorLayer::drawPanels() {}
void ImGuiEditorLayer::render(VkCommandBuffer) {}
bool ImGuiEditorLayer::wantsKeyboardCapture() const noexcept { return false; }

}  // namespace azurerender
#endif
