#pragma once

#include "render/RenderContext.hpp"

#include <ostream>
#include <sstream>
#include <string>
#include <string_view>

namespace azurerender {

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

    // Rebuilds swapchain-dependent resources after a swapchain recreate.
    virtual void onSwapchainRecreate(const RenderContext& context) = 0;

    // CPU-side per-frame update (animation, camera-relative uniforms, ...).
    virtual void updateFrame(const SceneFrameData& frame) = 0;

    // Records the scene passes into context.commandBuffer, writing the engine
    // Scene Color / depth / normal attachments through context.sceneFramebuffer.
    virtual void recordScene(const RenderContext& context) = 0;

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
    [[nodiscard]] virtual std::string_view diagnosticViewName(
        const std::uint32_t index) const noexcept {
        const SceneRendererCapabilities caps = capabilities();
        if (index < caps.diagnosticViewNames.size()) {
            return caps.diagnosticViewNames[index];
        }
        return "Unknown";
    }
};

}  // namespace azurerender
