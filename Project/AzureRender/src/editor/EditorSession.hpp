#pragma once

#include "EditorContext.hpp"
#include "editor/GameBuildJob.hpp"
#include "runtime/GameRuntime.hpp"
#include "runtime/LevelSession.hpp"
#include "runtime/ScriptRuntime.hpp"
#include "runtime/PresentationRuntime.hpp"

#include <memory>
#include <string>

namespace azurerender {

enum class EditorCommand {
    Save,
    ResetLayout,
    Reload,
    Undo,
    Redo,
    ReloadAssets,
    Capture,
    Play,
    Pause,
    Resume,
    Step,
    Stop,
};

class EditorSession final {
public:
    explicit EditorSession(std::shared_ptr<EditorContext> context);
    ~EditorSession();
    bool playing() const noexcept;
    GameRuntime* game() noexcept;
    RuntimeLifecycle* runtime() noexcept;
    LevelSession* levels() noexcept;
    ScriptRuntime* scripts() noexcept;
    PresentationRuntime* presentation() noexcept;
    double advance(double delta);
    SceneDocument viewScene();
    bool consumeRuntimeReset() noexcept;
    bool startBuild(const std::filesystem::path& install, const std::filesystem::path& output, bool replace = false) noexcept;
    void pollBuild();
    bool building() const noexcept { return build_ != nullptr; }
    const GameBuildResult& buildResult() const noexcept { return buildResult_; }

    [[nodiscard]] EditorContext& context() noexcept { return *context_; }
    [[nodiscard]] const EditorContext& context() const noexcept {
        return *context_;
    }
    [[nodiscard]] bool execute(EditorCommand command) noexcept;
    [[nodiscard]] bool saveOnClose() noexcept;
    [[nodiscard]] bool consumeLayoutResetRequest() noexcept;
    [[nodiscard]] bool consumeAssetReloadRequest() noexcept;
    [[nodiscard]] bool consumeCaptureRequest(std::string& label) noexcept;
    void setCaptureLabel(std::string label);
    [[nodiscard]] const std::string& captureLabel() const noexcept {
        return captureLabel_;
    }
    [[nodiscard]] const std::string& lastError() const noexcept {
        return lastError_;
    }

private:
    std::shared_ptr<EditorContext> context_;
    std::string lastError_;
    bool layoutResetRequested_ = false;
    bool assetReloadRequested_ = false;
    bool captureRequested_ = false;
    std::string captureLabel_ = "editor_capture";
    struct PlayState;
    std::unique_ptr<PlayState> play_;
    bool runtimeReset_ = false;
    std::unique_ptr<GameBuildJob> build_;
    GameBuildResult buildResult_;
};

}  // namespace azurerender
