#include "EditorSession.hpp"
#include "runtime/ModuleAssembly.hpp"

#include <exception>
#include <stdexcept>
#include <utility>

namespace azurerender {

struct EditorSession::PlayState {
    RuntimeLifecycle runtime;
    std::unique_ptr<LevelSession> levels;
    std::unique_ptr<GameRuntime> game;
    std::unique_ptr<PresentationRuntime> presentation;
    std::unique_ptr<ScriptRuntime> scripts;
    ModuleAssembly modules;
    explicit PlayState(EditorContext& context) {
        modules.add({"preview.document",1,{},{}}, [this,&context](auto&) {
        if (context.isProject()) levels = std::make_unique<LevelSession>(context.project(), runtime);
        runtime.replaceScene(context.scene(), [&](auto& target) {
            for (const auto& node : context.runtimeComponents()) installComponents(target.world(), target.entity(node.first), node.second);
        });
        }, [this] { levels.reset(); });
        modules.add({"preview.world",1,{}, {"preview.document"}}, [this](auto&) { runtime.start(); }, [this] { runtime.stop(); });
        modules.add({"preview.game",1,{}, {"preview.world"}}, [this](auto&) {
        game=std::make_unique<GameRuntime>(runtime);
        if(levels){presentation=std::make_unique<PresentationRuntime>(runtime,levels->assets());scripts=std::make_unique<ScriptRuntime>(runtime,*game,levels->assets());
            scripts->setAudioHandler([this](auto entity){presentation->play(entity);});
            scripts->setLevelHandler([this](std::string reference){levels->request(std::move(reference));});
            game->setBeforeStep([this](double delta){scripts->update(delta);});
            game->setEventHandler([this](const auto& event){scripts->dispatch(event);});
            game->setInteractionHandler([this](const auto& event){scripts->dispatchInteraction(event);});}
        }, [this] { scripts.reset(); game.reset(); presentation.reset(); });
        modules.start(runtime);
    }
};
EditorSession::~EditorSession()=default;
bool EditorSession::startBuild(const std::filesystem::path& install, const std::filesystem::path& output, bool replace) noexcept {
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
    if (!buildResult_.passed) lastError_ = buildResult_.message;
}
bool EditorSession::playing() const noexcept{return play_!=nullptr;}
GameRuntime* EditorSession::game() noexcept{return play_?play_->game.get():nullptr;}
RuntimeLifecycle* EditorSession::runtime() noexcept{return play_?&play_->runtime:nullptr;}
LevelSession* EditorSession::levels() noexcept{return play_?play_->levels.get():nullptr;}
ScriptRuntime* EditorSession::scripts() noexcept{return play_?play_->scripts.get():nullptr;}
PresentationRuntime* EditorSession::presentation() noexcept{return play_?play_->presentation.get():nullptr;}
double EditorSession::advance(double delta){if(!play_)return 0;if(play_->levels)play_->levels->poll();auto elapsed=play_->game->advance(delta);if(play_->presentation)play_->presentation->update(elapsed);return elapsed;}
SceneDocument EditorSession::viewScene(){return play_?play_->runtime.snapshotScene():context_->scene();}
bool EditorSession::consumeRuntimeReset() noexcept{const bool result=runtimeReset_;runtimeReset_=false;return result;}

EditorSession::EditorSession(std::shared_ptr<EditorContext> context)
    : context_(std::move(context)) {
    if (context_ == nullptr) {
        throw std::invalid_argument("Editor session requires a context");
    }
}

bool EditorSession::execute(const EditorCommand command) noexcept {
    lastError_.clear();
    if(building() && (command==EditorCommand::Save || command==EditorCommand::Reload || command==EditorCommand::Play || command==EditorCommand::Undo || command==EditorCommand::Redo)) {
        lastError_="Wait for game build to finish"; return false;
    }
    if(command==EditorCommand::Play || command==EditorCommand::Pause || command==EditorCommand::Resume || command==EditorCommand::Step || command==EditorCommand::Stop){
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
            context_->reload();
            return true;
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
