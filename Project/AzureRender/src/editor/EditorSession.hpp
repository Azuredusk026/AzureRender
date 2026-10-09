#pragma once

#include "EditorContext.hpp"
#include "properties/ReferencePickerService.hpp"
#include "projects/ProjectOpenService.hpp"
#include "documents/DocumentActionGuard.hpp"
#include "documents/EditorFeedback.hpp"
#include "documents/EditorTaskService.hpp"
#include "platform/PathSelection.hpp"
#include "viewport/GizmoController.hpp"
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
    void pollTasks();
    ProjectOpenService& projects() {return *projects_;}
    void setRecentProjectsPath(std::filesystem::path file) {projects_=std::make_unique<ProjectOpenService>(std::move(file));}
    std::string startProjectCreation(const std::string& templateId,const std::filesystem::path& destination,const std::string& name);
    void requestProject(std::shared_ptr<EditorContext> candidate);
    std::string consumeWorkspacePreset() {auto result=workspacePreset_;workspacePreset_.clear();return result;}

    void startImportTask(const std::filesystem::path& path);
    void cancelImportTask();
    const EditorTaskService& tasks() const{return *tasks_;}
    const EditorFeedback& feedback() const{return feedback_;}
    PathHistory& pathHistory(){return paths_;}
    void setPathHistoryFile(std::filesystem::path file){pathHistoryFile_=std::move(file);paths_.load(pathHistoryFile_);}
    void savePathHistory(){if(!pathHistoryFile_.empty())paths_.save(pathHistoryFile_);}
    using PlacementRaycast=std::function<std::optional<std::array<float,3>>(const std::array<float,3>&,const std::array<float,3>&)>;
    void setPlacementRaycast(PlacementRaycast query){placementRaycast_=std::move(query);}
    std::array<float,3> placementPosition(const nlohmann::json& parameters) const;
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
    std::string consumeAssetReveal(){auto result=assetReveal_;assetReveal_.clear();return result;}
    ReferencePickerService& references() noexcept {return references_;}
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
    void setClosePolicy(std::string policy){closePolicy_=std::move(policy);}
    DocumentActionGuard& documentGuard(){return *documentGuard_;}
    GizmoController& gizmo(){return *gizmo_;}
    bool requestDocumentAction(DocumentAction action);
    bool resolveDocumentAction(DocumentDecision decision);
    bool closeReady() const {return documentGuard_->state()==DocumentActionState::Ready && documentGuard_->action()==DocumentAction::Close;}
    bool consumeFrameSelection(){const bool result=frameSelectionRequested_;frameSelectionRequested_=false;return result;}
    [[nodiscard]] bool saveOnClose() noexcept;
    [[nodiscard]] bool consumeLayoutResetRequest() noexcept;
    [[nodiscard]] bool consumeAssetReloadRequest() noexcept;
    [[nodiscard]] bool consumeCaptureRequest(std::string& label) noexcept;
    void setCaptureLabel(std::string label);
    [[nodiscard]] const std::string& captureLabel() const noexcept {
        return captureLabel_;
    }
    [[nodiscard]] const std::string& lastError() const noexcept {
        return feedback_.latestError();
    }

private:
    friend EditRegistry editorOperations(EditorSession&);
    bool executeInternal(EditorCommand command) noexcept;
    bool startBuildInternal(const std::filesystem::path& install,const std::filesystem::path& output,bool replace) noexcept;
    void activatePendingProject();
    bool editEnabled(const EditDescriptor& descriptor) const;
    std::shared_ptr<EditorContext> context_;
    std::shared_ptr<EditorContext> pendingProject_;
    std::unique_ptr<ProjectOpenService> projects_;
    std::string workspacePreset_;
    bool creatingProject_=false;

    GeneratorRegistry generators_=GeneratorRegistry::builtins();
    DeveloperServices developerServices_;
    std::unique_ptr<EditService> edits_;
    std::unique_ptr<SelectionService> selection_;
    std::unique_ptr<GizmoController> gizmo_;
    SettingRegistry settings_;
    ReferencePickerService references_;
    std::string assetReveal_;
    std::unique_ptr<ModelClient> model_;
    std::unique_ptr<ProposalController> proposals_;
    EditorPanelRegistry panelRegistry_;
    std::filesystem::path userSettingsPath_;
    std::unique_ptr<DocumentActionGuard> documentGuard_;
    std::string closePolicy_="ask";
    bool frameSelectionRequested_=false;
    std::string lastError_;
    EditorFeedback feedback_;
    std::unique_ptr<EditorTaskService> tasks_;
    std::string importTask_;
    PathHistory paths_;
    std::filesystem::path pathHistoryFile_;
    PlacementRaycast placementRaycast_;
    void recordError(const std::string& source,const std::string& message);
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
