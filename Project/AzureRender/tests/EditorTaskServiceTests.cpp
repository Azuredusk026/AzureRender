#include "editor/EditorSession.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>
using namespace azurerender;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void wait(EditorSession& session) {
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
    while(session.context().importing()&&std::chrono::steady_clock::now()<deadline){
        session.advance(0);std::this_thread::sleep_for(std::chrono::milliseconds(5));}
    require(!session.context().importing(),"Session must finish imports without any open panel");
}
}
int main(int argc,char** argv){
    if(argc!=2)return 2;
    const auto root=std::filesystem::temp_directory_path()/("azure-tasks-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int status=0;
    try{
        Project::create(root,"Task workflow");
        auto context=EditorContext::openProject(root/"project.azureproject");EditorSession session(context);
        const auto original=context->scene().resources.size();
        require(static_cast<bool>(session.edit("asset.import-start",{{"path",argv[1]}})),"Real import must start through the production operation");
        wait(session);
        require(context->scene().resources.size()==original+1,"Closed-panel import must commit the registered resource");
        require(session.lastError().empty(),"Successful import must remain free of errors");
        const auto imported=context->documentContent();const auto undo=context->undoCount();
        require(static_cast<bool>(session.edit("asset.import-start",{{"path",argv[1]}})),"A second import must start after completion");
        require(static_cast<bool>(session.edit("asset.import-cancel")),"Import cancellation must be available");wait(session);
        require(context->documentContent()==imported&&context->undoCount()==undo,"Cancelled import must preserve authored content and history");
        require(session.lastError().empty(),"Explicit cancellation must have its own task state");
        require(static_cast<bool>(session.edit("asset.import-start",{{"path",(root/"missing.gltf").u8string()}})),"Background failures must be tracked after task start");wait(session);
        const auto message=session.lastError();require(!message.empty(),"Background failure must publish a persistent error");
        session.edit("viewport.gizmo-mode",{{"value",1}});
        require(session.lastError()==message,"Unrelated successful work must retain task diagnostics");
        const auto result=session.edit("tasks.describe");require(static_cast<bool>(result),"Task state must be queryable through a registered operation");
        require(result.value.size()==3,"Completed, cancelled and failed task results must remain observable");
        require(result.value[0].at("state")=="completed"&&result.value[1].at("state")=="cancelled"&&result.value[2].at("state")=="failed","Each task must report its own terminal state");
        context->save();const auto reopened=EditorContext::openProject(root/"project.azureproject");
        require(reopened->scene().resources.size()==original+1,"Committed import identity must survive save and reopen");
        std::cout<<"Panel-independent import, cancellation, failure and saved identity passed\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';status=1;}
    std::filesystem::remove_all(root);return status;
}
