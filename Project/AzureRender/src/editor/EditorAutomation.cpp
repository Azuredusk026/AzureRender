#include "editor/EditorAutomation.hpp"
#include <chrono>
#include <fstream>
namespace azurerender {
EditorAutomation::EditorAutomation(const std::filesystem::path& file){std::ifstream input(file);input>>actions_;if(!actions_.is_array())throw std::invalid_argument("Editor actions must be an array");std::uint64_t previous=0;for(const auto& action:actions_){auto frame=action.at("frame").get<std::uint64_t>();if(frame<previous)throw std::invalid_argument("Editor actions must be ordered by frame");previous=frame;}}
void EditorAutomation::advance(std::uint64_t frame,EditorSession& session){
    session.pollBuild();
    if(auto* scripts=session.scripts()){scriptErrors_=std::max(scriptErrors_,scripts->errors().size());if(scriptErrors_)recovered_=std::max(recovered_,scripts->activeCount());}
    while(cursor_<actions_.size()&&actions_[cursor_].at("frame").get<std::uint64_t>()<=frame){
        if(actions_[cursor_].at("command")=="wait-build" && session.building())return;
        const auto& action=actions_[cursor_++];const auto command=action.at("command").get<std::string>();auto start=std::chrono::steady_clock::now();bool passed=true;std::string error;
        try{auto& context=session.context();
            if(command=="build") { passed=session.startBuild(action.at("install").get<std::string>(),action.at("output").get<std::string>(),action.value("replace",false));error=session.lastError(); }
            else if(command=="wait-build") { passed=session.buildResult().passed;error=session.buildResult().message; }
            else if(command=="import")imported_=context.importAsset(action.at("path").get<std::string>());
            else if(command=="place")context.placeResource(imported_);
            else if(command=="rename")context.setSelectedNodeName(action.at("value").get<std::string>());
            else if(command=="duplicate")context.duplicateSelection();
            else if(command=="select")context.selectNodes(action.at("indices").get<std::vector<std::size_t>>());
            else if(command=="delete")context.deleteSelection();
            else if(command=="level"){if(!session.levels())throw std::logic_error("Level requires Play");session.levels()->request(action.at("value").get<std::string>());}
            else if(command=="write-script"){
                const auto path=context.project().resolve(action.at("path").get<std::string>());if(path.extension()!=".lua")throw std::invalid_argument("Script repair expects a Lua asset");
                std::ofstream output(path);output<<action.at("value").get<std::string>();output.close();if(!output)throw std::runtime_error("Cannot save script repair");
                if(session.scripts())session.scripts()->reloadChanged();
            }
            else{
                const std::map<std::string,EditorCommand> commands={{"save",EditorCommand::Save},{"undo",EditorCommand::Undo},{"redo",EditorCommand::Redo},{"play",EditorCommand::Play},{"pause",EditorCommand::Pause},{"resume",EditorCommand::Resume},{"step",EditorCommand::Step},{"stop",EditorCommand::Stop}};
                if(!commands.count(command))throw std::invalid_argument("Unknown editor action: "+command);
                if(command=="play")editBefore_=context.levelDocument();
                if(command=="stop"&&session.game()){steps_+=session.game()->steps();revision_=std::max(revision_,session.levels()?session.levels()->revision():0);}
                passed=session.execute(commands.at(command));error=session.lastError();
                if(command=="stop")restored_=restored_&&editBefore_==context.levelDocument();
            }
        }catch(const std::exception& exception){passed=false;error=exception.what();}
        results_.push_back({{"command",command},{"passed",passed},{"error",error},{"milliseconds",std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()}});
        if(!passed)throw std::runtime_error("Editor automation failed: "+command+": "+error);
    }
}
nlohmann::json EditorAutomation::report() const{return {{"editorActions",results_},{"editorPlaySteps",steps_},{"editorLevelRevision",revision_},{"editStateRestored",restored_},{"observedScriptErrors",scriptErrors_},{"recoveredScripts",recovered_}};}
}
