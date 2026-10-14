#include "editor/EditorSession.hpp"
#include "editor/EditorWorkspace.hpp"
#include "editor/projects/ProjectOpenService.hpp"
#include "editor/projects/ProjectCenterController.hpp"
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
        {
            EditorSession authored(first);
            const auto owner=first->selectedNode()->id;
            require(static_cast<bool>(authored.edit("component.add",{{"type","azure.rigid-body"}})),"Template permits physics authoring");
            require(static_cast<bool>(authored.edit("component.fields",{{"type","azure.rigid-body"},{"values",{{"kinematic",true},{"radius",.7}}}})),"Template accepts reusable physics parameters");
            require(static_cast<bool>(authored.edit("document.save")),"Template saves authored content");
            const auto reopened=EditorContext::openProject(first->project().file);
            const auto body=reopened->componentData(owner,"azure.rigid-body");
            require(body.is_object()&&body.at("kinematic")==true&&std::abs(body.at("radius").get<double>()-.7)<1e-6,"Registered component fields survive template save and reopen");
        }
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
        {
            ProjectCenterController center(root/"recent.json");
            const auto cached=center.recent();
            const auto source=std::filesystem::u8path(cached.at(0).at("path").get<std::string>());
            const auto moved=source.parent_path()/"temporarily-moved.azureproject";
            std::filesystem::rename(source,moved);
            require(center.recent()==cached,"Drawing recent projects reads a stable snapshot without disk probes");
            center.refreshRecent();
            require(!center.recent().at(0).at("available").get<bool>(),"Explicit refresh diagnoses moved projects");
            std::filesystem::rename(moved,source);
            center.forget(source.u8string());
            require(center.recent().size()==1&&std::filesystem::exists(source),"Forgetting history preserves project content");
            projects.remember(second->project());
        }
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
        for(const auto* type:{"scene","game"}){
            EditorSession closing(EditorContext::openProject(first->project().file));
            closing.setClosePolicy("discard");
            require(static_cast<bool>(closing.edit("project.create",{{"templateId",type},{"destination",(root/(std::string("close-")+type)).u8string()},{"name","Closing"}})),"Start independent background creation");
            const auto beforeDrag=closing.context().documentContent();
            if(std::string(type)=="scene"){
                auto begin=closing.edit("viewport.gizmo-begin");require(static_cast<bool>(begin),"Begin a drag while the project task is pending");
                auto matrix=begin.value.at("matrix").get<internal::Matrix4>();matrix[12]+=2;
                require(static_cast<bool>(closing.edit("viewport.gizmo-update",{{"matrix",matrix}})),"Apply an actual transform before closing");
            }
            require(static_cast<bool>(closing.edit("document.close")),"Window close must queue while project creation is active");
            closing.pollTasks();
            require(!closing.gizmo().active(),"Queued close must release a drag that blocks task activation");
            if(std::string(type)=="scene")require(closing.context().documentContent()==beforeDrag,"Window-close lifecycle must cancel and restore the drag");
            for(int i=0;i<1000&&!closing.closeReady();++i){closing.pollTasks();std::this_thread::sleep_for(std::chrono::milliseconds(2));}
            require(closing.closeReady()&&closing.context().project().name=="Closing","Queued close must complete after project activation");
        }
        {
            EditorSession cancelClose(EditorContext::openProject(first->project().file));
            require(static_cast<bool>(cancelClose.edit("node.rename",{{"value","Keep unsaved"}})),"Make close protection necessary");
            require(static_cast<bool>(cancelClose.edit("document.close")),"Dirty close requests a decision");
            require(cancelClose.documentGuard().state()==DocumentActionState::AwaitingDecision,"Dirty close must retain the prompt");
            require(static_cast<bool>(cancelClose.edit("document.decision",{{"value","cancel"}})),"User may cancel closing");
            for(int i=0;i<10;++i)cancelClose.pollTasks();
            require(!cancelClose.closeReady()&&cancelClose.documentGuard().state()==DocumentActionState::Idle&&cancelClose.context().dirty(),"Cancelled close must preserve content and remain cancelled");
            cancelClose.setClosePolicy("discard");
            require(static_cast<bool>(cancelClose.edit("document.close"))&&cancelClose.closeReady(),"A later independent close remains available");
        }
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
