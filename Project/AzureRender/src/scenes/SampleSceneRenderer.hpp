#pragma once

#include "extensions/ISceneRenderer.hpp"
#include "render/sky/SkyPass.hpp"

namespace azurerender {

// Empty scenes retain the shared world-oriented sky and host attachments.
class SampleSceneRenderer final : public ISceneRenderer {
public:
    [[nodiscard]] std::string_view name() const noexcept override {
        return "sample";
    }
    [[nodiscard]] SceneRendererCapabilities capabilities() const override;
    void onLoad(const RenderContext& context) override;
    bool reuseScene(const RenderContext& context) override;
    void onSwapchainRecreate(const RenderContext& context) override;
    void updateFrame(const SceneFrameData& frame) override;
    void recordScene(const RenderContext& context) override;
    void onUnload(const RenderContext& context) override;

private:
    SkyPass sky_;
    bool loaded_ = false;
};

}  // namespace azurerender
