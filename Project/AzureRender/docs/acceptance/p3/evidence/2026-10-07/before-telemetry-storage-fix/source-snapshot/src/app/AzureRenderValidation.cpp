#include "AzureRenderApp.hpp"
#include "runtime/LevelRenderSettings.hpp"
#if AZURE_WITH_EDITOR
#include "editor/ImGuiEditorLayer.hpp"
#include "editor/EditorSession.hpp"
#include "editor/EditorAutomation.hpp"
#endif
#include <cstdlib>
#include <fstream>
void AzureRenderApp::initializeValidation() {
    bool editorQueries=false;
#if AZURE_WITH_EDITOR
    editorQueries=editorAutomation_&&editorAutomation_->needsObservations();
#endif
    if(runOptions_.validationScript.empty()&&runOptions_.validationTokenEnvironment.empty()&&!editorQueries)return;
    using namespace azurerender;
    observations_=std::make_unique<ObservationRegistry>();
    observations_->add("engine.status",[]{return ObservationValue(std::string("ready"));});
    observations_->add("engine.frameCount",[this]{return ObservationValue(static_cast<std::int64_t>(gameplayFrame_));});
    observations_->add("engine.screenshots",[this]{return ObservationValue(static_cast<std::int64_t>(validationScreenshots_));});
    observations_->add("engine.gameInputSource",[this]{return ObservationValue(std::string(gameInputReplay_?"replay":"window"));});
    observations_->add("scene.nodeCount",[this]{
#if AZURE_WITH_EDITOR
        if(runOptions_.editorSession)return ObservationValue(static_cast<std::int64_t>(runOptions_.editorSession->viewScene().nodes.size()));
#endif
        return ObservationValue(static_cast<std::int64_t>(runtime_.snapshotScene().nodes.size()));
    });
    const auto settingDescriptors=engineSettings_->describe();
    for(const auto& field:settingDescriptors.items()) {
        const auto name=field.key();
        observations_->add("setting."+name,[this,name]{
            const auto value=engineSettings_->get(name);
            if(value.is_boolean())return ObservationValue(value.get<bool>());
            if(value.is_number_integer())return ObservationValue(value.get<std::int64_t>());
            if(value.is_number())return ObservationValue(value.get<double>());
            return ObservationValue(value.get<std::string>());
        });
        observations_->add("setting.source."+name,[this,name]{return ObservationValue(std::string(settingSourceName(engineSettings_->source(name))));});
    }
    observations_->add("render.exposure",[this]{return ObservationValue(static_cast<double>(effectiveRenderSettings_.grade.exposureEv));});
    observations_->add("render.diagnosticView",[this]{return ObservationValue(static_cast<std::int64_t>(effectiveRenderSettings_.diagnosticView));});
#if AZURE_WITH_EDITOR
    observations_->add("developer.status",[this]{return ObservationValue(previewOperation("developer.describe",{}).dump());});
    observations_->add("developer.shaderState",[this]{return ObservationValue(previewOperation("developer.describe",{}).at("shaderState").get<std::string>());});
    observations_->add("developer.shaderGeneration",[this]{return ObservationValue(static_cast<std::int64_t>(previewOperation("developer.describe",{}).at("shaderGeneration").get<std::uint64_t>()));});
#endif
    ValidationCallbacks callbacks;
#if AZURE_WITH_EDITOR
    if(auto session=runOptions_.editorSession) {
        observations_->add("document.id",[session]{return ObservationValue(session->edits().version().documentId);});
        observations_->add("document.revision",[session]{return ObservationValue(static_cast<std::int64_t>(session->edits().version().revision));});
        observations_->add("document.contentHash",[session]{return ObservationValue(session->edits().version().contentHash);});
        observations_->add("document.version",[session]{return ObservationValue(session->edits().version().describe().dump());});
        observations_->add("selection.id",[session]{const auto* node=session->context().selectedNode();return ObservationValue(node?node->id:std::string());});
        observations_->add("editor.playing",[session]{return ObservationValue(session->playing());});
        observations_->add("editor.building",[session]{return ObservationValue(session->building());});
        observations_->add("ai.state",[session]{return ObservationValue(session->proposalReport().at("state").get<std::string>());});
        observations_->add("ai.available",[session]{return ObservationValue(session->modelAvailable());});
        observations_->add("ai.proposal",[session]{return ObservationValue(session->proposalReport().dump());});
        callbacks.describe=[session](const auto&){return session->edits().describe();};
        callbacks.edit=[session](const nlohmann::json& request) {
            const auto& version=request.at("baseVersion");
            EditRequest edit{request.at("requestId").get<std::string>(),request.at("commandId").get<std::string>(),request.value("parameters",nlohmann::json::object()),
                {version.at("documentId").get<std::string>(),version.at("revision").get<std::uint64_t>(),version.at("contentHash").get<std::string>()},request.value("mergeKey",std::string())};
            const auto result=session->edits().execute(edit);
            if(!result)throw std::runtime_error(result.diagnostics.dump());
            return nlohmann::json{{"version",result.version.describe()},{"diff",result.diff},{"value",result.value}};
        };
    }
#endif
    callbacks.input=[this](const nlohmann::json& request) {
        const auto& event=request.at("event");
        std::string defaultDomain="game";
#if AZURE_WITH_EDITOR
        if(editorLayer_)defaultDomain="editor";
#endif
        const auto domain=request.value("domain",defaultDomain);
        if(domain=="editor") {
#if AZURE_WITH_EDITOR
            if(editorLayer_){editorLayer_->queueInputEvent(event);return nlohmann::json{{"queued",true},{"domain",domain}};}
#endif
            throw std::logic_error("Editor input requires an editor host");
        }
        if(domain!="game")throw std::invalid_argument("Unknown input domain");
        if(!activeGame())throw std::logic_error("Input requires an active project");
        if(!gameInputReplay_) {
            GameInputReplay queued;
            if(!runOptions_.gameActionsPath.empty()){std::ifstream file(runOptions_.gameActionsPath);nlohmann::json script;file>>script;queued=GameInputReplay::parse(script);}
            queued.enqueue(event,gameplayFrame_);gameInputReplay_=std::move(queued);
        }else gameInputReplay_->enqueue(event,gameplayFrame_);
        return nlohmann::json{{"queued",true},{"domain",domain}};
    };
    callbacks.capture=[this](const nlohmann::json& request) {
        const auto label=request.value("label",std::string("validation"));
        if(label.empty()||label.size()>128||label.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_")!=std::string::npos)
            throw std::invalid_argument("Screenshot label requires a portable basename");
        pendingScreenshotLabel_=label;screenshotRequested_=true;return nlohmann::json{{"label",label},{"scheduled",true}};
    };
    validation_=std::make_unique<ValidationService>(*observations_,std::move(callbacks));
#if AZURE_WITH_EDITOR
    if(editorAutomation_)editorAutomation_->setObservations([this](const auto& name){return observations_->query(name);});
#endif
    if(!runOptions_.validationScript.empty()) { std::ifstream file(runOptions_.validationScript);nlohmann::json script;file>>script;validation_->loadScript(script); }
    if(!runOptions_.validationTokenEnvironment.empty()) {
        const auto* token=std::getenv(runOptions_.validationTokenEnvironment.c_str());
        validationTransport_=std::make_unique<ValidationTransport>(*validation_,ValidationEndpoint{runOptions_.validationAddress,token?token:"",runOptions_.validationPort});
        std::ofstream endpoint(runOptions_.validationEndpoint);
        endpoint<<nlohmann::json{{"schemaVersion",1},{"address","127.0.0.1"},{"port",validationTransport_->port()}}.dump(2);
        if(!endpoint)throw std::runtime_error("Cannot write validation endpoint");
    }
}
void AzureRenderApp::finishValidation() {
    if(!validation_)return;
    if(!runOptions_.validationReport.empty()) {
        auto report=validation_->report();report["mode"]="production";report["fixedFrameStep"]=runOptions_.fixedFrameStep;
        report["fixedDescriptors"]=runOptions_.bindlessDisabled;report["cpuSkinning"]=runOptions_.computeSkinningDisabled;
        report["renderSettings"]=encodeLevelRenderSettings(effectiveRenderSettings_);
        report["window"]={{"width",swapchainExtent_.width},{"height",swapchainExtent_.height}};
        report["viewport"]={{"width",renderExtent_.width},{"height",renderExtent_.height}};
        std::ofstream file(runOptions_.validationReport);file<<report.dump(2);if(!file)throw std::runtime_error("Cannot write validation report");
    }
    if(validationTransport_)validationTransport_->stop();else validation_->stop();
    if(!validation_->report().at("passed").get<bool>())throw std::runtime_error("Validation script failed");
}
