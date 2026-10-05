#include "editor/EditorAutomation.hpp"
#include <chrono>
#include <fstream>
#include <set>
namespace azurerender {
EditorAutomation::EditorAutomation(const std::filesystem::path& file){std::ifstream input(file);input>>actions_;if(!actions_.is_array())throw std::invalid_argument("Editor actions must be an array");std::uint64_t previous=0;for(const auto& action:actions_){auto frame=action.at("frame").get<std::uint64_t>();if(frame<previous)throw std::invalid_argument("Editor actions must be ordered by frame");previous=frame;}}
void EditorAutomation::advance(std::uint64_t frame,EditorSession& session){
    session.pollBuild();
    if(auto* scripts=session.scripts()){scriptErrors_=std::max(scriptErrors_,scripts->errors().size());if(scriptErrors_)recovered_=std::max(recovered_,scripts->activeCount());}
    while(cursor_<actions_.size()&&actions_[cursor_].at("frame").get<std::uint64_t>()<=frame){
        if(actions_[cursor_].at("command")=="wait-build" && session.building())return;
        if(actions_[cursor_].at("command")=="wait-level" && session.levels()
            && session.levels()->lastError().empty()
            && session.levels()->currentReference()!=actions_[cursor_].at("value").get<std::string>())return;
        const auto& action=actions_[cursor_++];const auto command=action.at("command").get<std::string>();auto start=std::chrono::steady_clock::now();bool passed=true;std::string error;
        try{auto& context=session.context();
            auto edit=[&](const std::string& id,nlohmann::json args=nlohmann::json::object()) {
                auto result=session.edit(id,std::move(args));
                if(!result)throw std::runtime_error(session.lastError());
                return result.value;
            };
            if(command=="build") { passed=session.startBuild(action.at("install").get<std::string>(),action.at("output").get<std::string>(),action.value("replace",false));error=session.lastError(); }
            else if(command=="wait-build") { passed=session.buildResult().passed;error=session.buildResult().message; }
            else if(command=="import"){imported_=edit("asset.import",{{"path",action.at("path")}}).get<std::string>();if(action.contains("key"))imports_[action.at("key").get<std::string>()]=imported_;}
            else if(command=="place")edit("node.place",{{"resource",action.contains("resource")?imports_.at(action.at("resource").get<std::string>()):imported_},{"id",action.value("id",std::string())}});
            else if(command=="node")edit("node.create",{{"id",action.at("id")}});
            else if(command=="prefab")edit("prefab.place",{{"asset",action.at("asset")},{"instance",action.at("instance")}});
            else if(command=="component-add")edit("component.add",{{"type",action.at("type")}});
            else if(command=="component-field")edit("component.field",{{"type",action.at("type")},{"field",action.at("field")},{"value",action.at("value")}});
            else if(command=="transform"){
                auto fields=nlohmann::json::object();for(const char* field:{"translation","rotation","scale"})if(action.contains(field))fields[field]=action.at(field);
                edit("node.transform",std::move(fields));
            }
            else if(command=="preview")edit("animation.preview",{{"state",action.at("state")},{"time",action.value("time",0.0)},{"previous",action.value("previous",std::string())},{"crossfade",action.value("crossfade",0.0)}});
            else if(command=="clear-preview")edit("animation.clear-preview");
            else if(command=="debug-overlay")edit("viewport.debug-overlay",{{"enabled",action.at("enabled")}});
            else if(command=="rename")edit("node.rename",{{"value",action.at("value")}});
            else if(command=="duplicate")edit("node.duplicate");
            else if(command=="select"){
                if(action.contains("id"))edit("node.select",{{"id",action.at("id")}});
                else edit("node.select",{{"indices",action.at("indices")}});
            }
            else if(command=="delete")edit("node.delete");
            else if(command=="edit")edit(action.at("operation").get<std::string>(),action.value("parameters",nlohmann::json::object()));
            else if(command=="level")edit("preview.level",{{"value",action.at("value")}});
            else if(command=="wait-level"){
                if(!session.levels())throw std::logic_error("Level requires Play");
                if(!session.levels()->lastError().empty())throw std::runtime_error(session.levels()->lastError());
            }
            else if(command=="write-script")edit("script.write",{{"path",action.at("path")},{"value",action.at("value")}});
            else{
                const std::map<std::string,EditorCommand> commands={{"save",EditorCommand::Save},{"reload",EditorCommand::Reload},{"undo",EditorCommand::Undo},{"redo",EditorCommand::Redo},{"play",EditorCommand::Play},{"pause",EditorCommand::Pause},{"resume",EditorCommand::Resume},{"step",EditorCommand::Step},{"stop",EditorCommand::Stop}};
                if(!commands.count(command))throw std::invalid_argument("Unknown editor action: "+command);
                if(command=="play")editBefore_=context.levelDocument();
                if(command=="stop"&&session.game()){steps_+=session.game()->steps();revision_=std::max(revision_,session.levels()?session.levels()->revision():0);}
                passed=session.execute(commands.at(command));error=session.lastError();
                if(command=="stop")restored_=restored_&&editBefore_==context.levelDocument();
            }
        }catch(const std::exception& exception){passed=false;error=exception.what();}
        if(action.value("expectError",false)){passed=!passed;if(!passed)error="Expected rejection did not occur";}
        results_.push_back({{"command",command},{"passed",passed},{"error",error},{"milliseconds",std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()}});
        if(!passed)throw std::runtime_error("Editor automation failed: "+command+": "+error);
    }
}
nlohmann::json EditorAutomation::report() const{return {{"editorActions",results_},{"editorPlaySteps",steps_},{"editorLevelRevision",revision_},{"editStateRestored",restored_},{"observedScriptErrors",scriptErrors_},{"recoveredScripts",recovered_}};}
}
