#pragma once

#include "render/RenderContext.hpp"
#include "render/RenderSettings.hpp"

namespace azurerender {

// CPU frame input owns its settings. GPU handles remain owned by their frame
// resources and must stay alive until the submission fence retires.
class RenderFrameSnapshot final {
public:
    explicit RenderFrameSnapshot(const SceneFrameData& frame)
        : frame_(frame), settings_(frame.renderSettings != nullptr
              ? *frame.renderSettings : RenderSettings{}) {
        frame_.renderSettings = &settings_;
    }
    RenderFrameSnapshot(const RenderFrameSnapshot&) = delete;
    RenderFrameSnapshot& operator=(const RenderFrameSnapshot&) = delete;
    RenderFrameSnapshot(RenderFrameSnapshot&&) = delete;
    RenderFrameSnapshot& operator=(RenderFrameSnapshot&&) = delete;
    [[nodiscard]] const SceneFrameData& frame() const noexcept { return frame_; }
private:
    SceneFrameData frame_;
    const RenderSettings settings_;
};

}  // namespace azurerender
