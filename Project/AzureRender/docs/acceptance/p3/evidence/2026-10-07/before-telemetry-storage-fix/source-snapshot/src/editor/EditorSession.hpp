#pragma once

#include "EditorContext.hpp"
#include "editor/GameBuildJob.hpp"
#include "runtime/GameRuntime.hpp"
#include "runtime/LevelSession.hpp"
#include "runtime/IScriptRuntime.hpp"
#include "runtime/PresentationRuntime.hpp"
#include "editor/commands/EditService.hpp"
#include "editor/PanelContext.hpp"
#include "foundation/SettingRegistry.hpp"
#include "extensions/ExtensionRegistry.hpp"
#include "editor/ai/ProposalController.hpp"
#include "editor/preview/DeveloperServices.hpp"

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
    bool debugOverlay = false;
    GameRuntime* game() noexcept;
    RuntimeLifecycle* runtime() noexcept;
    LevelSession* levels() noexcept;
    IScriptRuntime* scripts() noexcept;
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
    EditService& edits() noexcept { return *edits_; }
    PanelContext panelContext() { return {*context_,*selection_,*edits_}; }
    void log(std::string message) { context_->log(std::move(message)); }
    void setUserSettingsPath(std::filesystem::path path) { userSettingsPath_=std::move(path); }
    const std::filesystem::path& userSettingsPath() const {return userSettingsPath_;}
    SettingRegistry& settings() noexcept { return settings_; }
    EditorPanelRegistry& panelRegistry() noexcept { return panelRegistry_; }
    SelectionService& selection() noexcept { return *selection_; }
    GeneratorRegistry& generators() noexcept { return generators_; }
    void setDeveloperServices(DeveloperServices services){developerServices_=std::move(services);}
    const DeveloperServices& developerServices() const{return developerServices_;}
    void configureModel(std::shared_ptr<IModelTransport> transport);
    bool modelAvailable() const noexcept {return model_&&model_->available();}
    void pollModel();
    nlohmann::json proposalReport() const;
    ProposalController& proposals();
    EditResult edit(const std::string& command,nlohmann::json parameters=nlohmann::json::object(),std::string mergeKey={});
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
    friend EditRegistry editorOperations(EditorSession&);
    bool executeInternal(EditorCommand command) noexcept;
    bool startBuildInternal(const std::filesystem::path& install,const std::filesystem::path& output,bool replace) noexcept;
    std::shared_ptr<EditorContext> context_;
    GeneratorRegistry generators_=GeneratorRegistry::builtins();
    DeveloperServices developerServices_;
    std::unique_ptr<EditService> edits_;
    std::unique_ptr<SelectionService> selection_;
    SettingRegistry settings_;
    std::unique_ptr<ModelClient> model_;
    std::unique_ptr<ProposalController> proposals_;
    EditorPanelRegistry panelRegistry_;
    std::filesystem::path userSettingsPath_;
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
