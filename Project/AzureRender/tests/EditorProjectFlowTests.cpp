#include "editor/EditorSession.hpp"
#include "editor/EditorWorkspace.hpp"
#include "editor/projects/ProjectOpenService.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>
using namespace azurerender;
int main() {
    const auto root=std::filesystem::temp_directory_path()/
        ("azure-project-flow-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root);
    try {
        auto require=[](bool condition,const char* message){if(!condition)throw std::runtime_error(message);};
        ProjectOpenService projects(root/"recent.json");
        const auto first=projects.create("scene",root/"中文项目","中文项目");
        const auto second=projects.create("scene",root/"second","Second");
        require(first->isProject()&&second->isProject(),"Templates must open as independent projects");
        bool rejected=false;
        try{projects.create("scene",root/"second","Duplicate");}catch(const std::exception&){rejected=true;}
        require(rejected,"Occupied destination must be rejected");
        rejected=false;
        try{projects.create("missing",root/"invalid","Invalid");}catch(const std::exception&){rejected=true;}
        require(rejected&&!std::filesystem::exists(root/"invalid"),"Invalid template must leave no output");
        rejected=false;
        try{projects.create("scene",root/"cancelled","Cancelled",[]{return true;});}catch(const std::exception&){rejected=true;}
        require(rejected&&!std::filesystem::exists(root/"cancelled"),"Cancelled creation must leave no output");
        int polls=0;rejected=false;
        try{projects.create("scene",root/"late-cancelled","Cancelled",[&]{return ++polls==2;});}catch(const std::exception&){rejected=true;}
        require(rejected&&!std::filesystem::exists(root/"late-cancelled"),"Cancellation after validation must reclaim outputs");
        for(const auto& entry:std::filesystem::directory_iterator(root))require(entry.path().filename().u8string().rfind(".azure-project-",0)!=0,"Temporary project must be reclaimed");
        projects.remember(first->project());projects.remember(second->project());
        ProjectOpenService restored(root/"recent.json");
        require(restored.recent().size()==2,"Recent projects must survive restart");
        std::filesystem::remove(second->project().file);
        require(!restored.recent()[0].at("available").get<bool>(),"Missing recent project must remain recoverable");
        std::filesystem::remove_all(root/"second");Project::create(root/"second","Second");
        EditorSession session(first);session.setRecentProjectsPath(root/"session-recent.json");
        require(static_cast<bool>(session.edit("node.rename",{{"value","Unsaved"}})),"Edit before switching");
        const auto identity=session.context().documentVersion().documentId;
        require(static_cast<bool>(session.edit("project.open",{{"path",(root/"second/project.azureproject").u8string()}})),"Open must enter document protection");
        session.pollTasks();
        require(session.context().documentVersion().documentId==identity,"Dirty project must await decision");
        session.edit("document.decision",{{"value","cancel"}});session.pollTasks();
        require(session.context().dirty()&&session.context().documentVersion().documentId==identity,"Cancel must preserve document");
        session.edit("project.open",{{"path",(root/"second/project.azureproject").u8string()}});
        session.edit("document.decision",{{"value","save"}});session.pollTasks();
        require(session.context().project().name=="Second"&&!session.context().canUndo(),"Switch must isolate history");
        require(EditorContext::openProject(first->project().file)->scene().nodes.front().name=="Unsaved","Save decision must persist source");
        require(!session.edit("project.open",{{"path",(root/"missing.azureproject").u8string()}}),"Failed open must preserve active project");
        require(session.context().project().name=="Second","Failure must preserve active project");
        const auto task=session.edit("project.create",{{"templateId","scene"},{"destination",(root/"async-project").u8string()},{"name","Async"}});
        require(static_cast<bool>(task)&&task.value.contains("task"),"Template creation must return a cancellable task");
        for(int i=0;i<1000&&session.context().project().name!="Async";++i){session.pollTasks();std::this_thread::sleep_for(std::chrono::milliseconds(2));}
        require(session.context().project().name=="Async","Completed template task must activate at a frame boundary");
        const auto cancel=session.edit("project.create",{{"templateId","scene"},{"destination",(root/"cancel-task").u8string()},{"name","Cancelled"}});
        require(static_cast<bool>(cancel),"Start cancellable creation");
        require(static_cast<bool>(session.edit("tasks.cancel",{{"id",cancel.value.at("task")}})),"Task cancellation uses public operation");
        for(int i=0;i<1000;++i){session.pollTasks();if(session.tasks().report().back().at("state")!="running")break;std::this_thread::sleep_for(std::chrono::milliseconds(2));}
        require(session.tasks().report().back().at("state")=="cancelled"&&!std::filesystem::exists(root/"cancel-task"),"Cancelled task must reclaim its project and preserve active document");
        EditorWorkspace workspace;workspace.registerPanel("custom","Custom");
        workspace.preset("authoring");require(!workspace.visible("capture"),"Authoring should focus content");
        workspace.preset("debugging");require(workspace.visible("gameplay-debug"),"Debugging should expose diagnostics");
        workspace.save(root/"layout");EditorWorkspace loaded;loaded.registerPanel("custom","Custom");
        require(loaded.load(root/"layout")&&loaded.snapshot().at("preset")=="debugging"&&loaded.visible("custom"),"Preset and extension must restore");
        rejected=false;try{loaded.preset("unknown");}catch(const std::exception&){rejected=true;}
        require(rejected&&loaded.snapshot().at("preset")=="debugging","Invalid preset must preserve workspace");
        std::filesystem::remove_all(root);return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';std::filesystem::remove_all(root);return 1;}
}
