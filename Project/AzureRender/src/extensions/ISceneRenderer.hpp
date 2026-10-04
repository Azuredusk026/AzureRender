#pragma once

#include "render/RenderContext.hpp"
#include "render/RenderGraph.hpp"

#include <ostream>
#include <sstream>
#include <string>
#include <string_view>

namespace azurerender {

struct SceneGraphResources {
    RenderGraph::ResourceId color;
    RenderGraph::ResourceId depth;
    RenderGraph::ResourceId normal;
    RenderGraph::ResourceId shadow;
};

// A pluggable scene renderer: the unit that draws a scene into the engine's
// HDR Scene Color attachment. The engine owns the swapchain, HDR composite,
// capture, timing and HUD; a scene renderer owns the scene passes, the shaders
// those passes need, and the GPU resources they consume.
//
// Register a concrete renderer through `SceneRendererRegistry` (see
// ExtensionRegistry.hpp). The engine selects one renderer per frame based on
// `RenderSettings::sceneType`.
class ISceneRenderer {
public:
    virtual ~ISceneRenderer() = default;

    // Stable registry id (e.g. "character", "blackhole").
    [[nodiscard]] virtual std::string_view name() const noexcept = 0;

    // Attachment/pass requirements and diagnostic view names. Called before
    // onLoad so the engine can prepare the scene framebuffer accordingly.
    [[nodiscard]] virtual SceneRendererCapabilities capabilities() const = 0;

    // Creates pipelines, descriptor sets, buffers and shaders. The context
    // device/extents/formats/render passes are valid from this call until
    // onUnload.
    virtual void onLoad(const RenderContext& context) = 0;
    virtual bool prepareLoad(const RenderContext& context,double budgetMs) {
        (void)budgetMs;onLoad(context);return true;
    }
    virtual bool reuseScene(const RenderContext& context) { (void)context;return false; }

    // Rebuilds swapchain-dependent resources after a swapchain recreate.
    virtual void onSwapchainRecreate(const RenderContext& context) = 0;

    // CPU-side per-frame update (animation, camera-relative uniforms, ...).
    // The owner completes this before registering/recording passes. No update,
    // unload or recreation may overlap graph recording or execution. Frame
    // inputs copied into renderer storage remain frozen until workers join.
    virtual void updateFrame(const SceneFrameData& frame) = 0;

    // Records the scene passes into context.commandBuffer, writing the engine
    // Scene Color / depth / normal attachments through context.sceneFramebuffer.
    virtual void recordScene(const RenderContext& context) = 0;

    // Register callbacks only. Borrowed handles and renderer storage must
    // outlive graph execution; GPU resources also outlive submission fences.
    virtual void registerPasses(RenderGraph& graph, const SceneGraphResources& resources,
                                const RenderContext& context) {
        const auto pass = graph.addCommandPass(std::string(name()), [this, context](rhi::ICommandRecorder& commands) {
            auto recordingContext = context;
            recordingContext.commands = &commands;
            recordScene(recordingContext);
        });
        graph.attachment(pass, resources.color, RenderGraphUsage::ColorAttachment,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        graph.attachment(pass, resources.depth, RenderGraphUsage::DepthAttachment,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);
        graph.attachment(pass, resources.normal, RenderGraphUsage::ColorAttachment,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    // Destroys every resource created in onLoad. The context remains valid.
    virtual void onUnload(const RenderContext& context) = 0;

    // Optional hook: appends scene-specific lines to the engine HUD panel.
    virtual void appendHudText(std::ostringstream& text) const {
        (void)text;
    }

    // Optional hook: standardized scene state for editor picking/gizmos.
    [[nodiscard]] virtual const RendererSceneState* sceneState()
        const noexcept {
        return nullptr;
    }

    // Optional hook: forwards host-level animation keys (F4/F11/7/8/9) to the
    // renderer when it owns animation state.
    virtual void onAnimationKey(const int key, const int action) {
        (void)key;
        (void)action;
    }

    // Optional hook: restarts scene playback (used by the portfolio orbit).
    virtual void restartPlayback() {}

    // Optional hook: forces playback paused/playing (used by the QA harness).
    virtual void setPlaybackPlaying(const bool playing) {
        (void)playing;
    }

    // Optional hook: appends scene-specific JSON fields (e.g. animation
    // index/name) to the capture manifest. Called while the manifest stream
    // is open; the renderer writes complete `"key": value,` lines.
    virtual void appendCaptureManifestFields(std::ostream& json) const {
        (void)json;
    }

    // Optional hook: maps a renderer-local diagnostic index to a display name.
    // Defaults to the capabilities list when the engine has one.
    //
    // capabilities() returns by value, so the returned string_view must not
    // point into that temporary. The names are copied into a function-local
    // thread_local cache whose storage outlives the call, keeping the view
    // valid for the caller.
    [[nodiscard]] virtual std::string_view diagnosticViewName(
        const std::uint32_t index) const noexcept {
        static thread_local std::string cachedName;
        const SceneRendererCapabilities caps = capabilities();
        if (index < caps.diagnosticViewNames.size()) {
            cachedName = caps.diagnosticViewNames[index];
            return cachedName;
        }
        return "Unknown";
    }
};

}  // namespace azurerender
