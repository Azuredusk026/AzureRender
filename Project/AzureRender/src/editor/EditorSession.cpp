#include "EditorSession.hpp"
#include "runtime/ModuleAssembly.hpp"
#include "runtime/EngineSettings.hpp"
#include "app/ProjectRuntimeAssembly.hpp"

#include <exception>
#include <stdexcept>
#include <utility>

namespace azurerender {

struct EditorSession::PlayState {
    RuntimeLifecycle runtime;
    std::unique_ptr<LevelSession> levels;
    std::unique_ptr<GameRuntime> game;
    std::unique_ptr<PresentationRuntime> presentation;
    std::unique_ptr<IScriptRuntime> scripts;
    ModuleAssembly modules;
    explicit PlayState(EditorContext& context) {
        modules.add({"preview.document",1,{},{}}, [this,&context](auto&) {
        if (context.isProject()) levels = std::make_unique<LevelSession>(context.project(), runtime);
        runtime.replaceScene(context.scene(), [&](auto& target) {
            for (const auto& node : context.runtimeComponents()) installComponents(target.world(), target.entity(node.first), node.second);
        });
        }, [this] { levels.reset(); });
        modules.add({"preview.world",1,{}, {"preview.document"}}, [this](auto&) { runtime.start(); }, [this] { runtime.stop(); });
        modules.add({"preview.game",1,{}, {"preview.world"}}, [this,&context](auto&) {
        game=std::make_unique<GameRuntime>(runtime,application::systems(),levels?application::configuration(context.project()):application::explorationConfiguration());
        if(levels){presentation=std::make_unique<PresentationRuntime>(runtime,levels->assets());scripts=application::scripts(context.project(),runtime,*game,levels->assets());
            scripts->setAudioHandler([this](auto entity){presentation->play(entity);});
            scripts->setLevelHandler([this](std::string reference){levels->request(std::move(reference));});
            game->setBeforeStep([this](double delta){scripts->fixedStep(delta);});
            game->setEventHandler([this](const auto& event){scripts->dispatch(event);});
            game->setInteractionHandler([this](const auto& event){scripts->dispatchInteraction(event);});}
        }, [this] { scripts.reset(); game.reset(); presentation.reset(); });
        modules.start(runtime);
    }
};
EditorSession::~EditorSession()=default;
void EditorSession::startImportTask(const std::filesystem::path& path) {
    context_->startImport(path);
    importTask_=tasks_->add("asset.import: "+path.u8string(),context_->project().file.u8string(),{
        [this]{return context_->importProgress();},[this]{return context_->importReady();},
        [this]{const auto result=edits_->current("asset.import-poll");if(!result)throw std::runtime_error(result.diagnostics.dump());return result.value;},
        [this]{context_->cancelImport();}});
}
void EditorSession::cancelImportTask(){if(!importTask_.empty())tasks_->cancel(importTask_);else context_->cancelImport();}
void EditorSession::pollTasks(){
    if(tasks_)tasks_->poll(!playing()&&!building()&&(!gizmo_||!gizmo_->active())&&documentGuard_->state()!=DocumentActionState::AwaitingDecision);
    pollBuild();
}
std::array<float,3> EditorSession::placementPosition(const nlohmann::json& parameters) const {
    const auto origin=parameters.at("origin").get<std::array<float,3>>();auto direction=parameters.at("direction").get<std::array<float,3>>();
    const auto length=internal::vectorLength(direction);if(length<1e-7F)throw EditRejection("Placement requires a nonzero ray direction");direction=internal::scaleVector(direction,1/length);
    if(placementRaycast_)if(const auto hit=placementRaycast_(origin,direction))return *hit;
    const auto plane=parameters.value("fallbackHeight",0.F);float distance=parameters.value("fallbackDistance",5.F);
    if(distance<=0||!std::isfinite(distance))throw EditRejection("Fallback distance must be positive and finite");
    if(std::abs(direction[1])>1e-7F){const auto intersection=(plane-origin[1])/direction[1];if(intersection>0)distance=intersection;}
    return internal::addVectors(origin,internal::scaleVector(direction,distance));
}
bool EditorSession::startBuildInternal(const std::filesystem::path& install, const std::filesystem::path& output, bool replace) noexcept {
    try {
        if (playing() || building() || !context_->isProject()) throw std::runtime_error("Build requires an idle game project in edit mode");
        context_->save();
        buildResult_ = {}; lastError_.clear();
        build_ = std::make_unique<GameBuildJob>(context_->project().file, install, output, replace);
        context_->log("Game build started"); return true;
    } catch (const std::exception& error) { lastError_ = error.what(); context_->log("ERROR: " + lastError_); return false; }
}
void EditorSession::pollBuild() {
    if (!build_ || !build_->ready()) return;
    buildResult_ = build_->finish(); build_.reset();
    context_->log((buildResult_.passed ? "Game build completed: " : "ERROR: Game build failed: ") + buildResult_.message);
    if (!buildResult_.passed) recordError("project.build",buildResult_.message);
}
bool EditorSession::playing() const noexcept{return play_!=nullptr;}
GameRuntime* EditorSession::game() noexcept{return play_?play_->game.get():nullptr;}
RuntimeLifecycle* EditorSession::runtime() noexcept{return play_?&play_->runtime:nullptr;}
LevelSession* EditorSession::levels() noexcept{return play_?play_->levels.get():nullptr;}
IScriptRuntime* EditorSession::scripts() noexcept{return play_?play_->scripts.get():nullptr;}
PresentationRuntime* EditorSession::presentation() noexcept{return play_?play_->presentation.get():nullptr;}
double EditorSession::advance(double delta){pollTasks();if(!play_)return 0;if(play_->levels)play_->levels->poll();auto elapsed=play_->game->advance(delta);if(play_->presentation)play_->presentation->update(elapsed);return elapsed;}
SceneDocument EditorSession::viewScene(){return play_?play_->runtime.snapshotScene():context_->scene();}
bool EditorSession::consumeRuntimeReset() noexcept{const bool result=runtimeReset_;runtimeReset_=false;return result;}

EditorSession::EditorSession(std::shared_ptr<EditorContext> context)
    : context_(std::move(context)) {
    if (context_ == nullptr) {
        throw std::invalid_argument("Editor session requires a context");
    }
    tasks_=std::make_unique<EditorTaskService>([this](const auto& source,const auto& message){recordError(source,message);});
    documentGuard_=std::make_unique<DocumentActionGuard>([this]{return context_->dirty();},[this]{context_->save();});
    edits_=std::make_unique<EditService>(*context_,editorOperations(*this),[this](const EditDescriptor& descriptor) {
        if(gizmo_&&gizmo_->active()&&(descriptor.requiresIdle||descriptor.modifiesDocument||descriptor.id=="node.select"||descriptor.id=="selection.click"||descriptor.id=="document.save"||descriptor.id.rfind("preview.",0)==0))
            if(descriptor.id!="viewport.gizmo-update"&&descriptor.id!="viewport.gizmo-cancel"&&descriptor.id!="viewport.gizmo-commit")return false;
        return !(descriptor.requiresIdle||descriptor.modifiesDocument)||(!playing()&&!building());
    });
    registerEngineSettings(settings_);
    registerModelSettings(settings_);
    settings_.add({"editor.scale","Interface scale multiplier",1.0,.75,3.,false,true,false});
    settings_.add({"editor.compact","Compact workspace",false,{},{},false,true,false});
    settings_.add({"editor.camera.invertX","Invert horizontal editor camera input",false,{},{},false,true,false});
    settings_.add({"editor.camera.invertY","Invert vertical editor camera input",false,{},{},false,true,false});
    settings_.add({"editor.camera.flySpeed","Editor flight speed",5.,.01,10000.,false,true,false});
    settings_.add({"editor.camera.boost","Editor flight acceleration",4.,1.,100.,false,true,false});
    settings_.add({"editor.camera.orbitSensitivity","Editor orbit sensitivity",1.,.01,10.,false,true,false});
    for(const auto& binding:std::vector<std::pair<const char*,const char*>>{{"move","W"},{"rotate","E"},{"scale","R"},{"select","Q"},{"frame","F"}})
        settings_.add({std::string("editor.shortcuts.")+binding.first,"Scene shortcut (one uppercase letter)",binding.second,{},{},false,true,false});
    selection_=std::make_unique<SelectionService>(*context_,*edits_);
    gizmo_=std::make_unique<GizmoController>(*context_);
    settings_.add({"editor.gizmo.snap","Snap gizmo transforms",false,{},{},false,true,false});
    settings_.add({"editor.gizmo.moveStep","Gizmo translation snap step",.5,.001,10000.,false,true,false});
    settings_.add({"editor.gizmo.rotateStep","Gizmo rotation snap degrees",15.,.1,90.,false,true,false});
    settings_.add({"editor.gizmo.scaleStep","Gizmo scale snap step",.1,.001,10.,false,true,false});
}
void EditorSession::configureModel(std::shared_ptr<IModelTransport> transport){
    if(playing()||building()||(proposals_&&proposals_->state()==ProposalState::Generating))throw std::logic_error("Model assembly requires an idle session");
    proposals_.reset();model_.reset();
    if(!settings_.get("ai.enabled").get<bool>()||!transport)return;
    model_=std::make_unique<ModelClient>(std::move(transport));
    proposals_=std::make_unique<ProposalController>(*context_,*edits_,generators_,*model_);
}
void EditorSession::pollModel(){if(proposals_)proposals_->poll();}
nlohmann::json EditorSession::proposalReport() const{return proposals_?proposals_->report():nlohmann::json{{"state","Disabled"},{"available",false}};}
ProposalController& EditorSession::proposals(){if(!proposals_)throw EditRejection("Optional content assistance is disabled");return *proposals_;}

EditResult EditorSession::edit(const std::string& command,nlohmann::json parameters,std::string mergeKey) {
    auto result=edits_->current(command,std::move(parameters),std::move(mergeKey));
    if(!result) {
        lastError_=result.diagnostics.empty()?"Edit failed":result.diagnostics.front().value("message",std::string("Edit rejected"));
        recordError(command,lastError_);
    }
    return result;
}
void EditorSession::recordError(const std::string& source,const std::string& message){lastError_=message;feedback_.record(source,message);context_->log("ERROR ["+source+"]: "+message);}
bool EditorSession::startBuild(const std::filesystem::path& install,const std::filesystem::path& output,bool replace) noexcept {
    try { return static_cast<bool>(edit("project.build",{{"install",install.u8string()},{"output",output.u8string()},{"replace",replace}})); }
    catch(const std::exception& error) { recordError("editor.command",error.what());return false; }
}
bool EditorSession::execute(const EditorCommand command) noexcept {
    static const std::map<EditorCommand,std::string> ids={{EditorCommand::Save,"document.save"},{EditorCommand::Reload,"document.reload"},
        {EditorCommand::Undo,"history.undo"},{EditorCommand::Redo,"history.redo"},{EditorCommand::Play,"preview.play"},
        {EditorCommand::Pause,"preview.pause"},{EditorCommand::Resume,"preview.resume"},{EditorCommand::Step,"preview.step"},
        {EditorCommand::Stop,"preview.stop"},{EditorCommand::ResetLayout,"workspace.reset"},{EditorCommand::ReloadAssets,"assets.reload"},
        {EditorCommand::Capture,"viewport.capture"}};
    try { return static_cast<bool>(edit(ids.at(command))); }
    catch(const std::exception& error) { recordError("editor.command",error.what());return false; }
}
bool EditorSession::executeInternal(const EditorCommand command) noexcept {
    lastError_.clear();
    if(building() && (command==EditorCommand::Save || command==EditorCommand::Reload || command==EditorCommand::Play || command==EditorCommand::Undo || command==EditorCommand::Redo)) {
        lastError_="Wait for game build to finish"; return false;
    }
    if(command==EditorCommand::Play || command==EditorCommand::Pause || command==EditorCommand::Resume || command==EditorCommand::Step || command==EditorCommand::Stop){
        context_->closeEditMerge();
        try{
            if(command==EditorCommand::Play){if(play_)return false;context_->detachRenderSettings();play_=std::make_unique<PlayState>(*context_);runtimeReset_=true;context_->log("Play started");}
            else if(command==EditorCommand::Stop){if(!play_)return false;play_.reset();runtimeReset_=true;context_->log("Play stopped; edit state restored");}
            else{if(!play_)return false;if(command==EditorCommand::Pause)play_->runtime.pause();else if(command==EditorCommand::Resume)play_->runtime.resume();else play_->runtime.step();}
            return true;
        }catch(const std::exception& exception){lastError_=exception.what();context_->log("ERROR: "+lastError_);return false;}
    }
    if(play_ && (command==EditorCommand::Save || command==EditorCommand::Reload || command==EditorCommand::Undo || command==EditorCommand::Redo))return false;
    if (command == EditorCommand::ResetLayout) {
        layoutResetRequested_ = true;
        context_->log("Default editor layout requested");
        return true;
    }
    if (command == EditorCommand::Undo) {
        return context_->undo();
    }
    if (command == EditorCommand::Redo) {
        return context_->redo();
    }
    if (command == EditorCommand::ReloadAssets) {
        const std::size_t changed = context_->reloadChangedAssets();
        assetReloadRequested_ = true;
        context_->log("Asset reload requested; changed files: "
            + std::to_string(changed));
        return true;
    }
    if (command == EditorCommand::Capture) {
        captureRequested_ = true;
        context_->log("Viewport capture requested: " + captureLabel_);
        return true;
    }
    try {
        if (command == EditorCommand::Reload) {
            return requestDocumentAction(DocumentAction::Reload);
        }
        context_->save();
        return true;
    } catch (const std::exception& exception) {
        lastError_ = exception.what();
    } catch (...) {
        lastError_ = "Unknown editor command failure";
    }
    context_->log("ERROR: " + lastError_);
    return false;
}

bool EditorSession::requestDocumentAction(DocumentAction action){
    if(gizmo_&&gizmo_->active())gizmo_->cancel();
    auto state=documentGuard_->request(action);
    if(state==DocumentActionState::AwaitingDecision && closePolicy_!="ask")
        return resolveDocumentAction(closePolicy_=="save"?DocumentDecision::Save:DocumentDecision::Discard);
    if(state==DocumentActionState::Ready && action==DocumentAction::Reload){
        try{context_->reload();documentGuard_->reset();}catch(const std::exception& error){recordError("document.reload",error.what());return false;}
    }
    return state!=DocumentActionState::Failed;
}
bool EditorSession::resolveDocumentAction(DocumentDecision decision){
    const auto state=documentGuard_->resolve(decision);
    if(state==DocumentActionState::Failed){recordError("document.decision",documentGuard_->diagnostic());return false;}
    if(state==DocumentActionState::Ready && documentGuard_->action()==DocumentAction::Reload){
        try{context_->reload();documentGuard_->reset();}catch(const std::exception& error){recordError("document.reload",error.what());return false;}
    }
    return true;
}
bool EditorSession::saveOnClose() noexcept {
    if(play_)static_cast<void>(execute(EditorCommand::Stop));
    return !context_->dirty() || execute(EditorCommand::Save);
}

bool EditorSession::consumeLayoutResetRequest() noexcept {
    const bool requested = layoutResetRequested_;
    layoutResetRequested_ = false;
    return requested;
}

bool EditorSession::consumeAssetReloadRequest() noexcept {
    const bool requested = assetReloadRequested_;
    assetReloadRequested_ = false;
    return requested;
}

bool EditorSession::consumeCaptureRequest(std::string& label) noexcept {
    if (!captureRequested_) {
        return false;
    }
    captureRequested_ = false;
    label = captureLabel_;
    return true;
}

void EditorSession::setCaptureLabel(std::string label) {
    if (label.empty()) {
        label = "editor_capture";
    }
    for (char& character : label) {
        const bool valid = (character >= 'a' && character <= 'z')
            || (character >= 'A' && character <= 'Z')
            || (character >= '0' && character <= '9')
            || character == '-' || character == '_';
        if (!valid) {
            character = '_';
        }
    }
    captureLabel_ = std::move(label);
}

}  // namespace azurerender
