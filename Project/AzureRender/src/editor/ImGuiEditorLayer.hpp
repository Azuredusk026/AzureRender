#pragma once

#include "EditorContext.hpp"
#include "EditorCameraController.hpp"
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
    void setViewportImages(
        VkSampler sampler,
        const std::vector<VkImageView>& imageViews,
        std::uint32_t width,
        std::uint32_t height);
    void setViewportImageIndex(std::uint32_t imageIndex);
    [[nodiscard]] EditorViewportInput consumeViewportInput() noexcept;
    [[nodiscard]] bool acceptsViewportShortcuts() const noexcept {
        return viewportAcceptsShortcuts_;
    }

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
    std::vector<VkDescriptorSet> viewportTextures_;
    std::uint32_t viewportImageIndex_ = 0;
    std::uint32_t viewportWidth_ = 1;
    std::uint32_t viewportHeight_ = 1;
    EditorViewportInput viewportInput_;
    bool viewportFocused_ = false;
    bool viewportAcceptsShortcuts_ = false;
    bool initialized_ = false;
    bool dockingLayoutInitialized_ = false;
};

}  // namespace azurerender
