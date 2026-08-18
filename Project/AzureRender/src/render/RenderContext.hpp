#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include "render/EnvironmentAsset.hpp"

// Defined in assets/GltfLoader.hpp (global namespace).
struct LoadedAsset;

namespace azurerender {

struct RenderSettings;

// Standardized scene state the engine can read from a scene renderer for
// editor integration (picking, gizmos, HUD). A renderer without pickable
// geometry (e.g. the blackhole renderer) leaves the pointer null.
struct RendererSceneState {
    const LoadedAsset* asset = nullptr;
    const float* modelMatrix = nullptr;  // 16 floats, column-major
    std::int32_t selectedPrimitiveIndex = -1;
    std::size_t primitiveCount = 0;
};

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

inline void validateSceneRendererCapabilities(
    const SceneRendererCapabilities& capabilities) {
    if (capabilities.diagnosticViewNames.empty()
        || capabilities.diagnosticViewNames.front() != "Beauty") {
        throw std::invalid_argument(
            "Scene renderer diagnostic view 0 must be Beauty");
    }
}

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
    bool captureActive = false;
    bool technicalSequence = false;
    std::uint32_t technicalSequenceChapter =
        std::numeric_limits<std::uint32_t>::max();
    std::uint32_t currentFrame = 0;

    // Editor interaction state the engine forwards for highlight/gizmo use.
    std::int32_t selectedPrimitiveIndex = -1;
    float gizmoTranslation[3]{0.0F, 0.0F, 0.0F};
    float gizmoRotation[3]{0.0F, 0.0F, 0.0F};
    float gizmoScale[3]{1.0F, 1.0F, 1.0F};
    bool gizmoActive = false;

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

    VkRenderPass sceneRenderPass = VK_NULL_HANDLE;
    VkRenderPass postProcessRenderPass = VK_NULL_HANDLE;
    VkPipelineLayout postProcessPipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout postProcessDescriptorSetLayout = VK_NULL_HANDLE;
    VkSampler screenAttachmentSampler = VK_NULL_HANDLE;

    // Engine-owned shadow map (fixed resolution, engine-sampled by the
    // post-process Shadow Map diagnostic). A scene renderer records into it
    // through its own shadow pipeline; renderers without a shadow pass may
    // leave the map unrendered (contents undefined until cleared).
    VkFormat shadowFormat = VK_FORMAT_UNDEFINED;
    VkRenderPass shadowRenderPass = VK_NULL_HANDLE;
    VkFramebuffer shadowFramebuffer = VK_NULL_HANDLE;
    VkImageView shadowImageView = VK_NULL_HANDLE;
    VkSampler shadowSampler = VK_NULL_HANDLE;
    std::uint32_t shadowMapSize = 2048;

    // Current in-flight scene framebuffer the renderer records into.
    VkFramebuffer sceneFramebuffer = VK_NULL_HANDLE;

    // Asset the renderer loads on onLoad (resolved absolute path).
    std::string assetPath;
    // Directory holding compiled .spv shaders for renderer pipelines.
    std::string shaderDirectory;
    // Scene-independent environment source. Renderers may sample the shared
    // equirectangular representation or provide their own GPU convolution.
    SceneEnvironmentSource environment;
    std::string rampAtlasPath;

    // GPU timing hooks: the renderer writes timestamps 0..2 around its scene
    // passes when gpuTimingEnabled is true; the engine writes the final one.
    VkQueryPool timestampQueryPool = VK_NULL_HANDLE;
    std::uint32_t timestampQueryCount = 0;
    bool gpuTimingEnabled = false;

    const RenderSettings* renderSettings = nullptr;
};

}  // namespace azurerender
