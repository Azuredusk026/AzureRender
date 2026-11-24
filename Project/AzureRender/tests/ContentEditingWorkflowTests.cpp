#include "editor/EditorSession.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>
using namespace azurerender;
int main(){const auto root=std::filesystem::temp_directory_path()/("azure-content-edit-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));try{
 const auto require=[](bool value,const char* message){if(!value)throw std::runtime_error(message);};
 Project::create(root,"File workflow");std::ofstream(root/"assets/sample.txt")<<"sample";
 auto context=EditorContext::openProject(root/"project.azureproject");EditorSession session(context);
 const auto id=context->assets().idForPath("assets:/sample.txt");
 auto planned=session.edit("asset.rename",{{"asset",id},{"name","edited.txt"},{"dryRun",true}});
 require(static_cast<bool>(planned)&&std::filesystem::exists(root/"assets/sample.txt"),"UI planning must preserve source files");
 require(static_cast<bool>(session.edit("asset.rename",{{"planId",planned.value.at("id")}})),"UI must apply the same plan");
 require(context->assets().idForPath("assets:/edited.txt")==id,"Active content database must refresh after publication");
 require(static_cast<bool>(session.edit("content.undo")),"Public file undo must restore the journal");
 require(context->assets().idForPath("assets:/sample.txt")==id,"File undo must refresh the active database");
 auto dependencies=session.edit("asset.dependencies",{{"asset",id},{"direction","reverse"}});require(static_cast<bool>(dependencies)&&dependencies.value.at("assets").empty(),"Dependency queries must use stable identities");
 const auto beforeDiagnostic=context->documentVersion();
 const auto inspected=session.edit("asset.inspect",{{"asset",id}});
 require(static_cast<bool>(inspected)&&inspected.value.at("id")==id&&inspected.value.at("ready")==true,"Asset diagnostics must expose the registered source and identity");
 require(context->documentVersion()==beforeDiagnostic,"Read-only asset diagnostics must preserve the document version");
 const auto fingerprint=std::to_string(context->assets().records().at(id).fingerprint);
 auto reimport=session.edit("asset.reimport",{{"asset",id},{"settings",nlohmann::json::object()},{"baseFingerprint",fingerprint}});
 require(static_cast<bool>(reimport)&&reimport.value.contains("task"),"Reimport must return a cancellable asynchronous task");
 require(!session.edit("asset.rename",{{"asset",id},{"name","busy.txt"}}),"Active reimport must arbitrate content publication");
 require(static_cast<bool>(session.edit("tasks.cancel",{{"id",reimport.value.at("task")}})),"Reimport cancellation must use the common task service");
 for(unsigned i=0;i<2000;++i){session.pollTasks();if(session.tasks().report().back().at("state")!="running")break;std::this_thread::sleep_for(std::chrono::milliseconds(1));}
 require(session.tasks().report().back().at("state")=="cancelled","Cancelled reimport must complete without publication");
 require(std::to_string(context->assets().records().at(id).fingerprint)==fingerprint,"Cancellation must preserve the active fingerprint");
 require(!session.edit("asset.reimport",{{"asset",id},{"settings",nlohmann::json::object()},{"baseFingerprint","expired"}}),"Reimport must reject expired origins");
 auto successful=session.edit("asset.reimport",{{"asset",id},{"settings",nlohmann::json::object()},{"baseFingerprint",fingerprint}});require(static_cast<bool>(successful),"Reimport must restart after cancellation");
 for(unsigned i=0;i<2000;++i){session.pollTasks();if(session.tasks().report().back().at("state")!="running")break;std::this_thread::sleep_for(std::chrono::milliseconds(1));}
 require(session.tasks().report().back().at("state")=="completed","Validated reimport must publish on the host");
 auto staleTask=session.edit("asset.reimport",{{"asset",id},{"settings",nlohmann::json::object()},{"baseFingerprint",fingerprint}});require(static_cast<bool>(staleTask),"Reimport must prepare before an external change");
 std::ofstream(root/"assets/sample.txt")<<"external during task";
 for(unsigned i=0;i<2000;++i){session.pollTasks();if(session.tasks().report().back().at("state")!="running")break;std::this_thread::sleep_for(std::chrono::milliseconds(1));}
 require(session.tasks().report().back().at("state")=="failed","Reimport must reject external input changes before publication");
 session.selection().set({context->scene().nodes.front().id});session.edit("node.rename",{{"value","Unsaved"}});
 auto unsafe=session.edit("asset.rename",{{"asset",id},{"name","unsafe.txt"}});
 require(!unsafe&&std::filesystem::exists(root/"assets/sample.txt"),"Content writes must protect unsaved scene changes");
 std::filesystem::remove_all(root);return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';std::filesystem::remove_all(root);return 1;}}
