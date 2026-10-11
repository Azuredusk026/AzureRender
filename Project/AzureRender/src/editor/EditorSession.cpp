#include "EditorSession.hpp"
#include "EditorWorkspace.hpp"
#include "AssetImportJob.hpp"
#include "assets/AssetExportJob.hpp"
#include "projects/ProjectCreateJob.hpp"
#include "runtime/ModuleAssembly.hpp"
#include "runtime/EngineSettings.hpp"
#include "app/ProjectRuntimeAssembly.hpp"
#include "gameplay/SharedSystems.hpp"
#include "runtime/physics/PhysicsShapeFactory.hpp"

#include <exception>
#include <stdexcept>
#include <utility>

namespace azurerender {

struct EditorSession::PlayState {
    bool simulation=false;
    RuntimeLifecycle runtime;
    std::unique_ptr<LevelSession> levels;
    std::unique_ptr<GameRuntime> game;
    std::unique_ptr<PresentationRuntime> presentation;
    std::unique_ptr<IScriptRuntime> scripts;
    ModuleAssembly modules;
    explicit PlayState(EditorContext& context,bool simulate=false):simulation(simulate) {
        modules.add({"preview.document",1,{},{}}, [this,&context](auto&) {
        if (context.isProject()&&!simulation) levels = std::make_unique<LevelSession>(context.project(), runtime);
        runtime.replaceScene(context.scene(), [&](auto& target) {
            for (const auto& node : context.runtimeComponents()) installComponents(target.world(), target.entity(node.first), node.second);
        });
        }, [this] { levels.reset(); });
        modules.add({"preview.world",1,{}, {"preview.document"}}, [this](auto&) { runtime.start(); }, [this] { runtime.stop(); });
        modules.add({"preview.game",1,{}, {"preview.world"}}, [this,&context](auto&) {
        if(simulation){SystemRegistry registry;gameplay::registerSharedSystems(registry);
            game=std::make_unique<GameRuntime>(runtime,registry,nlohmann::json{{"schemaVersion",1},{"input",nlohmann::json::object()},{"systems",nlohmann::json::array({{{"id","physics"},{"config",nlohmann::json::object()}}})}});
        }else game=std::make_unique<GameRuntime>(runtime,application::systems(),levels?application::configuration(context.project()):application::explorationConfiguration());
        if(context.isProject())game->physics().setMeshResolver(collisionMeshAssets(levels?levels->assets():context.assets()));
        game->physics().validateScene(runtime);
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
ContentOperationService& EditorSession::content() {
    if(!context_->isProject())throw EditRejection("Content operations require a project");
    if(!content_)content_=std::make_unique<ContentOperationService>(context_->project());
    const auto& database=context_->assets();
    if(contentDatabase_!=&database||contentDatabaseVersion_!=database.version()){
        content_->synchronize(database);contentDatabase_=&database;contentDatabaseVersion_=database.version();
    }return *content_;
}
nlohmann::json EditorSession::applyContent(const std::string& planId) {
    contentDirectories_.cancelRefresh();
    if(context_->importing()||reimporting_)throw EditRejection("Finish the content import task first");
    if(context_->dirty())throw EditRejection("Save scene changes before applying content operations");
    references_.cancel();const auto result=content().apply(planId);
    try{context_->reloadProjectContent();}
    catch(...){const auto failure=std::current_exception();content().undo();context_->reloadProjectContent();std::rethrow_exception(failure);}
    assetReloadRequested_=true;return result;
}
void EditorSession::undoContent() {
    contentDirectories_.cancelRefresh();
    if(context_->importing()||reimporting_)throw EditRejection("Finish the content import task first");
    if(context_->dirty())throw EditRejection("Save scene changes before undoing content operations");
    references_.cancel();content().undo();context_->reloadProjectContent();assetReloadRequested_=true;
}
void EditorSession::redoContent() {
    contentDirectories_.cancelRefresh();
    if(context_->importing()||reimporting_||context_->dirty())throw EditRejection("Content redo requires a saved idle project");
    references_.cancel();content().redo();context_->reloadProjectContent();assetReloadRequested_=true;
}
std::string EditorSession::startReimport(const nlohmann::json& request) {
    contentDirectories_.cancelRefresh();
    if(context_->dirty()||context_->importing()||reimporting_)throw EditRejection("Reimport requires a saved idle project");
    auto parameters=request;parameters["operation"]="asset.reimport";auto plan=content().plan(parameters);
    const auto id=plan.at("asset").get<std::string>(),planId=plan.at("id").get<std::string>();
    nlohmann::json materialMapping;
    try{materialMapping=MaterialEditingService::reimportSlots(*context_,id);
        if(!materialMapping.at("slotDiagnostics").empty())throw EditRejection(materialMapping.at("slotDiagnostics").dump());
    }catch(...){content().discard(planId);throw;}
    const auto& database=content().database();std::set<std::string> visited;std::vector<AssetRecord> inputs;
    const auto collect=[&](const std::string& current,auto&& self)->void {
        if(!visited.insert(current).second)return;const auto& record=database.records().at(current);inputs.push_back(record);
        for(const auto& dependency:record.dependencies)self(dependency,self);
    };collect(id,collect);
    auto job=std::make_shared<AssetReimportJob>(database.records().at(id),std::move(inputs),plan.at("settings"));
    const auto task=tasks_->add("asset.reimport",id,{
        [job]{return job->progress();},[job]{return job->ready();},
        [this,job,planId,id,materialMapping] {
            try{auto result=job->finish();const auto current=MaterialEditingService::reimportSlots(*context_,id);
                if(current!=materialMapping)throw EditRejection("Source material slots changed during reimport");
                result.update(materialMapping);reimporting_=false;result["transaction"]=applyContent(planId);return result;}
            catch(...){reimporting_=false;content().discard(planId);throw;}
        },[job]{job->cancel();}});
    reimporting_=true;return task;
}

void EditorSession::startImportTask(const std::filesystem::path& path) {
    contentDirectories_.cancelRefresh();
    context_->startImport(path);
    importTask_=tasks_->add("asset.import: "+path.u8string(),context_->project().file.u8string(),{
        [this]{return context_->importProgress();},[this]{return context_->importReady();},
        [this]{const auto result=edits_->current("asset.import-poll");if(!result)throw std::runtime_error(result.diagnostics.dump());return result.value;},
        [this]{context_->cancelImport();},
        [this,path](const auto& message){recordError("asset.import",message,{{"path",path.u8string()}});}});
}
nlohmann::json EditorSession::planExport(const nlohmann::json& request){
    if(playing()||building()||reimporting_)throw EditRejection("Export planning requires an idle editor");return exports_.plan(*context_,request);
}
std::string EditorSession::startExport(const std::string& plan,bool replaceExisting){
    if(playing()||building()||reimporting_)throw EditRejection("Export requires an idle editor");
    auto request=exports_.consume(*context_,plan);auto job=std::make_shared<AssetExportJob>(request);const auto context=context_;const auto revision=context->revision();const auto databaseVersion=context->assets().version();
    return tasks_->add("asset.export",request.destination.u8string(),{
        [job]{return job->progress();},[job]{return job->ready();},
        [this,job,context,revision,databaseVersion,replaceExisting]{
            if(context_!=context||context->dirty()||context->revision()!=revision||context->assets().version()!=databaseVersion)throw EditRejection("Export document changed before publication");
            const auto output=job->finish(replaceExisting);auto result=job->report().summary;result["output"]=output.u8string();return result;
        },[job]{job->cancel();}});
}
void EditorSession::cancelImportTask(){if(!importTask_.empty())tasks_->cancel(importTask_);else context_->cancelImport();}
void EditorSession::pollTasks(){
    pollDocuments();
    activatePendingProject();
    if(tasks_)tasks_->poll(!playing()&&!building()&&(!gizmo_||!gizmo_->active())&&documentGuard_->state()!=DocumentActionState::AwaitingDecision);
    pollBuild();
    pollWindowClose();
    collectDiagnostics();
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
bool EditorSession::simulating() const noexcept{return play_&&play_->simulation;}
GameRuntime* EditorSession::game() noexcept{return play_?play_->game.get():nullptr;}
RuntimeLifecycle* EditorSession::runtime() noexcept{return play_?&play_->runtime:nullptr;}
LevelSession* EditorSession::levels() noexcept{return play_?play_->levels.get():nullptr;}
IScriptRuntime* EditorSession::scripts() noexcept{return play_?play_->scripts.get():nullptr;}
PresentationRuntime* EditorSession::presentation() noexcept{return play_?play_->presentation.get():nullptr;}
double EditorSession::advance(double delta){pollTasks();if(!play_){animations().advance(*context_,delta);return 0;}if(play_->levels)play_->levels->poll();auto elapsed=play_->game->advance(delta);if(play_->presentation)play_->presentation->update(elapsed);collectDiagnostics();return elapsed;}
std::vector<NodeAnimationFrame> EditorSession::animationFrames() const{
    auto result=documents_.at(context_->documentId()).animations.frames();
    if(const auto& pose=context_->animationPreview();pose&&std::none_of(result.begin(),result.end(),[&](const auto& frame){return frame.node==pose->node;}))result.push_back(*pose);return result;
}
SceneDocument EditorSession::viewScene(){return play_?play_->runtime.snapshotScene():context_->scene();}
bool EditorSession::consumeRuntimeReset() noexcept{const bool result=runtimeReset_;runtimeReset_=false;return result;}

EditorSession::EditorSession(std::shared_ptr<EditorContext> context)
    : context_(std::move(context)) {
    if (context_ == nullptr) {
        throw std::invalid_argument("Editor session requires a context");
    }
    projects_=std::make_unique<ProjectOpenService>(EditorWorkspace::configDirectory()/"recent-projects.json");
    tasks_=std::make_unique<EditorTaskService>([this](const auto& source,const auto& message){recordError(source,message);});
    documentGuard_=std::make_unique<DocumentActionGuard>([this]{return context_->dirty()||(documentGuard_->action()==DocumentAction::Close&&documents_.dirtyExcept(context_->documentId()));},[this]{if(documentGuard_->action()==DocumentAction::Close)documents_.saveAll();else context_->save();});
    edits_=std::make_unique<EditService>(*context_,editorOperations(*this),[this](const EditDescriptor& descriptor){return editEnabled(descriptor);});
    registerEngineSettings(settings_);
    registerModelSettings(settings_);
    settings_.add({"editor.scale","Interface scale multiplier",1.0,.75,3.,false,true,false});
    settings_.add({"editor.compact","Compact workspace",false,{},{},false,true,false});
    settings_.add({"editor.camera.invertX","Invert horizontal editor camera input",false,{},{},false,true,false});
    settings_.add({"editor.camera.invertY","Invert vertical editor camera input",false,{},{},false,true,false});
    settings_.add({"editor.camera.flySpeed","Editor flight speed",5.,.01,10000.,false,true,false});
    settings_.add({"editor.thumbnailSize","Content thumbnail size",96,64,192,false,true,false});
    settings_.add({"editor.camera.boost","Editor flight acceleration",4.,1.,100.,false,true,false});
    settings_.add({"editor.camera.rawMouse","Use raw mouse motion during captured flight",true,{},{},false,true,false});
    settings_.add({"editor.camera.orbitSensitivity","Editor orbit sensitivity",1.,.01,10.,false,true,false});
    for(const auto& binding:std::vector<std::pair<const char*,const char*>>{{"move","W"},{"rotate","E"},{"scale","R"},{"select","Q"},{"frame","F"}})
        settings_.add({std::string("editor.shortcuts.")+binding.first,"Scene shortcut (one uppercase letter)",binding.second,{},{},false,true,false});
    selection_=std::make_unique<SelectionService>(*context_,*edits_);
    gizmo_=std::make_unique<GizmoController>(*context_);
    settings_.add({"editor.gizmo.snap","Snap gizmo transforms",false,{},{},false,true,false});
    settings_.add({"editor.gizmo.moveStep","Gizmo translation snap step",.5,.001,10000.,false,true,false});
    settings_.add({"editor.gizmo.rotateStep","Gizmo rotation snap degrees",15.,.1,90.,false,true,false});
    settings_.add({"editor.gizmo.scaleStep","Gizmo scale snap step",.1,.001,10.,false,true,false});
    retainDocument();
}
bool EditorSession::editEnabled(const EditDescriptor& descriptor) const {
    if(descriptor.id.rfind("document.",0)==0&&descriptor.id!="document.decision"&&descriptor.id!="document.close"&&(context_->importing()||creatingProject_||pendingProject_||!pendingDocument_.empty()||!closingDocument_.empty()))return false;
    if(reimporting_&&(descriptor.requiresIdle||descriptor.modifiesDocument))return false;
    if(gizmo_&&gizmo_->active()&&(descriptor.requiresIdle||descriptor.modifiesDocument||descriptor.id=="node.select"||descriptor.id=="selection.click"||descriptor.id=="document.save"||descriptor.id.rfind("preview.",0)==0))
        if(descriptor.id!="viewport.gizmo-update"&&descriptor.id!="viewport.gizmo-cancel"&&descriptor.id!="viewport.gizmo-commit")return false;
    if(simulating()&&!descriptor.modifiesDocument&&!building()&&(descriptor.id=="viewport.orientation"||descriptor.id=="viewport.projection"||descriptor.id=="viewport.fly-speed"||descriptor.id=="viewport.frame-selection"||descriptor.id=="viewport.overlay-policy"))return true;
    return !(descriptor.requiresIdle||descriptor.modifiesDocument)||(!playing()&&!building());
}
std::string EditorSession::startProjectCreation(const std::string& templateId,const std::filesystem::path& destination,const std::string& name) {
    if(creatingProject_||playing()||building()||context_->importing()||reimporting_||gizmo_->active()||pendingProject_||
        (proposals_&&proposals_->state()==ProposalState::Generating)||documentGuard_->state()!=DocumentActionState::Idle)
        throw EditRejection("Finish the active task before creating a project");
    auto job=std::make_shared<ProjectCreateJob>(templateId,destination,name);
    const auto id=tasks_->add("project.create",destination.u8string(),{
        [job]{return job->progress();},[job]{return job->ready();},
        [this,job]{
            creatingProject_=false;
            try{auto candidate=job->finish();const auto path=candidate->project().file.u8string();requestProject(std::move(candidate));return nlohmann::json{{"path",path},{"pending",true}};}
            catch(...){creatingProject_=false;throw;}
        },[job]{job->cancel();}});
    creatingProject_=true;return id;
}
void EditorSession::requestProject(std::shared_ptr<EditorContext> candidate) {
    if(!candidate||!candidate->isProject())throw EditRejection("Prepared project is required");
    if(documents_.dirtyExcept(context_->documentId()))throw EditRejection("Save or close other modified documents before switching project");
    if(creatingProject_||playing()||building()||context_->importing()||reimporting_||gizmo_->active()||pendingProject_||
        (proposals_&&proposals_->state()==ProposalState::Generating)||
        documentGuard_->state()==DocumentActionState::AwaitingDecision||documentGuard_->state()==DocumentActionState::Failed)
        throw EditRejection("Finish the active task before switching project");
    pendingProject_=std::move(candidate);
    if(!requestDocumentAction(DocumentAction::Open)){pendingProject_.reset();throw EditRejection("Project switch protection failed");}
}
void EditorSession::activatePendingProject() {
    if(!pendingProject_)return;
    if(documentGuard_->state()==DocumentActionState::Idle){pendingProject_.reset();return;}
    if(documentGuard_->state()!=DocumentActionState::Ready||documentGuard_->action()!=DocumentAction::Open)return;
    try {
        auto edits=std::make_unique<EditService>(*pendingProject_,editorOperations(*this),[this](const EditDescriptor& descriptor){return editEnabled(descriptor);});
        auto selection=std::make_unique<SelectionService>(*pendingProject_,*edits);
        auto gizmo=std::make_unique<GizmoController>(*pendingProject_);
        std::unique_ptr<ProposalController> proposals;
        if(model_)proposals=std::make_unique<ProposalController>(*pendingProject_,*edits,generators_,*model_);
        projects_->remember(pendingProject_->project());
        references_.cancel();content_.reset();contentDirectories_.reset();materials_.clear();prefabs_.clear();context_->detachRenderSettings();proposals_.reset();
        selection_=std::move(selection);gizmo_=std::move(gizmo);edits_=std::move(edits);
        context_=std::move(pendingProject_);proposals_=std::move(proposals);documents_.clear();retainDocument();documentGuard_->reset();runtimeReset_=true;assetReloadRequested_=true;
    }catch(const std::exception& error){pendingProject_.reset();documentGuard_->reset();recordError("project.open",error.what());}
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
    if(command=="node.select"||command=="selection.click"||command.rfind("preview.",0)==0||command=="document.reload")references_.cancel();
    auto result=edits_->current(command,parameters,std::move(mergeKey));
    if(!result) {
        lastError_=result.diagnostics.empty()?"Edit failed":result.diagnostics.front().value("message",std::string("Edit rejected"));
        recordError(command,lastError_,parameters);
    }
    return result;
}
void EditorSession::recordError(const std::string& source,const std::string& message,const nlohmann::json& parameters){
    lastError_=message;feedback_.record(source,message);context_->log("ERROR ["+source+"]: "+message);
    StructuredDiagnostic diagnostic;diagnostic.source=source;diagnostic.code="OperationFailed";diagnostic.message=message;diagnostic.document=context_->documentId();
    if(source.rfind("component.",0)==0&&context_->selectedNode())diagnostic.node=context_->selectedNode()->id;
    if(parameters.contains("path")&&parameters.at("path").is_string())diagnostic.path=parameters.at("path").get<std::string>();
    if(parameters.contains("asset")&&parameters.at("asset").is_string()&&context_->isProject()){
        const auto asset=parameters.at("asset").get<std::string>();if(context_->assets().records().count(asset)){diagnostic.asset=asset;if(diagnostic.path.empty())diagnostic.path=context_->assets().records().at(asset).path.u8string();}
    }
    diagnostics_.record(std::move(diagnostic));
}
EditResult EditorSession::editRequest(const EditRequest& request){
    if(request.commandId=="node.select"||request.commandId=="selection.click"||request.commandId.rfind("preview.",0)==0||request.commandId=="document.reload")references_.cancel();
    auto result=edits_->execute(request);
    if(!result)recordError(request.commandId,result.diagnostics.empty()?"Edit rejected":result.diagnostics.front().value("message",std::string("Edit rejected")),request.parameters);
    return result;
}
bool EditorSession::startBuild(const std::filesystem::path& install,const std::filesystem::path& output,bool replace) noexcept {
    try { return static_cast<bool>(edit("project.build",{{"install",install.u8string()},{"output",output.u8string()},{"replace",replace}})); }
    catch(const std::exception& error) { recordError("editor.command",error.what());return false; }
}
bool EditorSession::execute(const EditorCommand command) noexcept {
    static const std::map<EditorCommand,std::string> ids={{EditorCommand::Save,"document.save"},{EditorCommand::Reload,"document.reload"},
        {EditorCommand::Undo,"history.undo"},{EditorCommand::Redo,"history.redo"},{EditorCommand::Play,"preview.play"},{EditorCommand::Simulate,"preview.simulate"},
        {EditorCommand::Pause,"preview.pause"},{EditorCommand::Resume,"preview.resume"},{EditorCommand::Step,"preview.step"},
        {EditorCommand::Stop,"preview.stop"},{EditorCommand::ResetLayout,"workspace.reset"},{EditorCommand::ReloadAssets,"assets.reload"},
        {EditorCommand::Capture,"viewport.capture"}};
    try { return static_cast<bool>(edit(ids.at(command))); }
    catch(const std::exception& error) { recordError("editor.command",error.what());return false; }
}
bool EditorSession::executeInternal(const EditorCommand command) noexcept {
    lastError_.clear();
    if(building() && (command==EditorCommand::Save || command==EditorCommand::Reload || command==EditorCommand::Play || command==EditorCommand::Simulate || command==EditorCommand::Undo || command==EditorCommand::Redo)) {
        lastError_="Wait for game build to finish"; return false;
    }
    if(command==EditorCommand::Play || command==EditorCommand::Simulate || command==EditorCommand::Pause || command==EditorCommand::Resume || command==EditorCommand::Step || command==EditorCommand::Stop){
        context_->closeEditMerge();
        try{
            if(command==EditorCommand::Play||command==EditorCommand::Simulate){if(play_)return false;auto candidate=std::make_unique<PlayState>(*context_,command==EditorCommand::Simulate);context_->detachRenderSettings();play_=std::move(candidate);runtimeReset_=true;context_->log(simulating()?"Simulation started":"Play started");}
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

bool EditorSession::requestWindowClose(){
    if(documentGuard_->state()!=DocumentActionState::Idle&&documentGuard_->action()==DocumentAction::Close)
        return documentGuard_->state()!=DocumentActionState::Failed;
    const bool wasPending=windowClosePending_;
    windowClosePending_=true;
    const bool accepted=(gizmo_&&gizmo_->active())||pollWindowClose();
    if(windowClosePending_&&!wasPending)context_->log("Window close pending: waiting for active document tasks");
    return accepted;
}
bool EditorSession::pollWindowClose(){
    if(!windowClosePending_)return true;
    references_.cancel();
    if(gizmo_&&gizmo_->active())gizmo_->cancel();
    if(context_->importing()||creatingProject_||pendingProject_||reimporting_||building()||
       !pendingDocument_.empty()||!closingDocument_.empty()||documentGuard_->state()!=DocumentActionState::Idle)return true;
    windowClosePending_=false;
    return requestDocumentAction(DocumentAction::Close);
}
bool EditorSession::requestDocumentAction(DocumentAction action){
    references_.cancel();
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
    try{documents_.saveAll();return true;}catch(const std::exception& error){recordError("document.save-all",error.what());return false;}
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
