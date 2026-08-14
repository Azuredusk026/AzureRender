#pragma once

#include "EditorContext.hpp"
#include "IEditorPanel.hpp"

#include <vulkan/vulkan.h>

#include <memory>
#include <vector>

struct GLFWwindow;

namespace azurerender {

class ImGuiEditorLayer final {
public:
    explicit ImGuiEditorLayer(std::shared_ptr<EditorContext> context);
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
        std::uint32_t imageCount);
    void shutdownVulkan();
    void newFrame();
    void drawPanels();
    void render(VkCommandBuffer commandBuffer);
    [[nodiscard]] bool wantsKeyboardCapture() const noexcept;

private:
    void drawViewportPanel();
    void drawOutlinerPanel();
    void drawInspectorPanel();
    void drawAssetBrowserPanel();
    void drawConsolePanel();

    std::shared_ptr<EditorContext> context_;
    std::vector<std::unique_ptr<IEditorPanel>> panels_;
    VkDevice device_ = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;
    bool initialized_ = false;
    bool dockingLayoutInitialized_ = false;
};

}  // namespace azurerender
