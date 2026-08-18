#pragma once

#include "extensions/ISceneRenderer.hpp"

namespace azurerender {

// Minimal SDK example. It owns no GPU objects and only clears the attachments
// supplied by the host, making lifecycle and ownership boundaries explicit.
class SampleSceneRenderer final : public ISceneRenderer {
public:
    [[nodiscard]] std::string_view name() const noexcept override {
        return "sample";
    }
    [[nodiscard]] SceneRendererCapabilities capabilities() const override;
    void onLoad(const RenderContext& context) override;
    void onSwapchainRecreate(const RenderContext& context) override;
    void updateFrame(const SceneFrameData& frame) override;
    void recordScene(const RenderContext& context) override;
    void onUnload(const RenderContext& context) override;

private:
    bool loaded_ = false;
};

}  // namespace azurerender
