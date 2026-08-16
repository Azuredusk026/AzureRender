#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace azurerender {

struct RenderSettings;

// Declares what a scene renderer needs from the engine-owned frame.
// The engine uses this to decide which attachments/passes exist before the
// renderer is loaded, so a scene renderer can never silently depend on a
// resource the engine does not provide.
struct SceneRendererCapabilities {
    static constexpr std::uint32_t kApiVersion = 1;

    bool requiresSceneDepth = true;
    bool requiresSceneNormal = true;
    // Diagnostic view names shown by the engine HUD / technical sequence.
    // Index 0 must be the Beauty (final composite) name.
    std::vector<std::string> diagnosticViewNames;
};

// Per-frame state the engine hands to every scene renderer. Scene-specific
// simulation state (camera, animation, QA flags) is forwarded here so the
// engine stays the single owner of host-level input while the renderer owns
// the scene rendering itself.
struct SceneFrameData {
    float deltaSeconds = 0.0F;
    double timeSeconds = 0.0;

    const RenderSettings* renderSettings = nullptr;

    float cameraPosition[3]{0.0F, 0.0F, 0.0F};
    float cameraTarget[3]{0.0F, 0.0F, 0.0F};
    float rotationAngle = 0.0F;

    std::uint32_t qaIsolationMode = 0;
    std::uint32_t qaEffectMode = 0;
    bool qaEffectEnabled = true;
    bool qaHarnessEnabled = false;

    std::uint64_t capturedFrames = 0;
    std::uint32_t captureFps = 60;
    bool technicalSequence = false;
    std::uint32_t technicalSequenceChapter =
        std::numeric_limits<std::uint32_t>::max();

    std::uint32_t swapchainWidth = 0;
    std::uint32_t swapchainHeight = 0;
};

// Read-only Vulkan context the engine provides to a scene renderer for the
// lifetime of its load. All objects except the per-frame command buffer and
// framebuffer are owned by the engine and outlive the renderer; the renderer
// must never destroy them.
struct RenderContext {
    VkDevice device = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    std::uint32_t graphicsQueueFamily = 0;
    VkCommandPool commandPool = VK_NULL_HANDLE;

    std::uint32_t maxFramesInFlight = 2;
    std::uint32_t currentFrame = 0;
    std::uint32_t imageIndex = 0;
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;

    VkExtent2D renderExtent{};
    VkExtent2D swapchainExtent{};
    VkFormat sceneColorFormat = VK_FORMAT_UNDEFINED;
    VkFormat depthFormat = VK_FORMAT_UNDEFINED;
    VkFormat normalFormat = VK_FORMAT_R8G8B8A8_UNORM;
    VkFormat shadowFormat = VK_FORMAT_UNDEFINED;
    std::uint32_t shadowMapSize = 2048;

    VkRenderPass sceneRenderPass = VK_NULL_HANDLE;
    VkRenderPass postProcessRenderPass = VK_NULL_HANDLE;
    VkPipelineLayout postProcessPipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout postProcessDescriptorSetLayout = VK_NULL_HANDLE;
    VkSampler screenAttachmentSampler = VK_NULL_HANDLE;

    // Current in-flight scene framebuffer the renderer records into.
    VkFramebuffer sceneFramebuffer = VK_NULL_HANDLE;

    const RenderSettings* renderSettings = nullptr;
};

}  // namespace azurerender
