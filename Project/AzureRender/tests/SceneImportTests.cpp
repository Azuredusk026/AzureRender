#include "assets/scene/SceneExchange.hpp"
#include "editor/scene/SceneImportService.hpp"
#include "editor/EditorSession.hpp"
#include "editor/commands/EditTransaction.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>
using namespace azurerender;
using Json=nlohmann::json;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void write(const std::filesystem::path& path,const Json& value){std::ofstream(path)<<value.dump(2);}
Json run(EditorSession& session,const std::string& command,Json args=Json::object()){auto result=session.edit(command,args);if(!result)throw std::runtime_error(command+": "+result.diagnostics.dump());return result.value;}
Json fixture(){return {{"schemaVersion",1},{"id","shared-room"},{"unitMeters",.01},{"upAxis","Y"},{"resources",Json::array({{{"id","panel"},{"path","panel.gltf"}}})},{"nodes",Json::array({{{"id","root"},{"localTransform",{{"translation",{100,0,0}}}}},{{"id","left"},{"parent","root"},{"meshRef","panel"},{"localTransform",{{"translation",{0,200,0}},{"scale",{-1,2,1}}}}},{{"id","right"},{"parent","root"},{"meshRef","panel"}}})}};}
}
int main(int argc,char** argv){if(argc<2||argc>3)return 2;const auto root=std::filesystem::temp_directory_path()/("azure-scene-exchange-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));const auto moved=std::filesystem::path(root.u8string()+"-moved");try{
 Project::create(root,"Exchange");std::filesystem::copy_file(argv[1],root/"assets/panel.gltf");const auto source=root/"assets/room.azuresceneexchange";auto data=fixture();write(source,data);
 const auto proposal=readSceneExchange(source);require(proposal.scene.nodes.size()==3&&proposal.scene.resources.size()==1,"Shared instance resources remain shared");require(proposal.scene.nodes[0].translation[0]==1&&proposal.scene.nodes[1].translation[1]==2&&proposal.scene.nodes[1].scale[0]==-1,"Units, hierarchy and mirrored scale preserved");
 {auto lit=data;lit["lights"]=Json::array({{{"id","sun"},{"node","root"},{"type","directional"},{"shape",{{"direction",{0,0,-1}}}}}});write(source,lit);auto imported=readSceneExchange(source);
 require(imported.scene.lights.size()==1&&imported.scene.lights[0].shape.type==static_cast<LightKind>(2),"Scene exchange preserves directional light identity");write(source,data);}
 {auto bad=data;bad["nodes"][1]["id"]="unrepresentable-panel";bad["nodes"][1]["localTransform"]["matrix"]={1,0,0,0,.1,1,0,0,0,0,1,0,0,0,0,1};write(source,bad);
 bool contextual=false;try{readSceneExchange(source);}catch(const std::invalid_argument& error){const std::string message=error.what();contextual=message.find("unrepresentable-panel")!=std::string::npos&&message.find("shear")!=std::string::npos;}
 require(contextual,"Matrix rejection must retain invalid_argument, source node identity and original cause");write(source,data);}
 for(unsigned failure=0;failure<5;++failure){auto bad=data;if(failure==0)bad["schemaVersion"]=2;if(failure==1)bad["nodes"][1]["id"]="root";if(failure==2)bad["nodes"][0]["parent"]="left";if(failure==3)bad["resources"][0]["path"]="absent.gltf";if(failure==4)bad["resources"][0]["path"]="../outside.gltf";write(source,bad);bool rejected=false;try{readSceneExchange(source);}catch(const std::exception&){rejected=true;}require(rejected,"Invalid input rejected before mutation");}write(source,data);
 bool cancelled=false;try{readSceneExchange(source,[]{return true;});}catch(const std::exception&){cancelled=true;}require(cancelled,"Cancelled parse rejected");
 write(root/"assets/main.azurelevel",{{"schemaVersion",1},{"id","exchange-test"},{"sceneType","sample"},{"resources",Json::array()},{"nodes",Json::array()},{"lights",Json::array()}});
 Json project;{std::ifstream stream(root/"project.azureproject");stream>>project;}project["startupScene"]="assets:/main.azurelevel";write(root/"project.azureproject",project);
 auto context=EditorContext::openProject(root/"project.azureproject");EditorSession session(context);const auto before=context->documentContent();
 {auto changed=data;changed["nodes"][0]["name"]="Changed after parse";write(source,changed);EditTransaction transaction(*context);bool rejected=false;try{SceneImportService::apply(transaction.candidate(),proposal);}catch(const std::exception&){rejected=true;}require(rejected&&context->documentContent()==before,"Stale source fingerprint rejects complete transaction");write(source,data);}
 const auto imported=run(session,"scene.import",{{"path",source.u8string()}});const auto mapping=imported.at("nodeMap");const auto left=mapping.at("left").get<std::string>();const auto after=context->documentContent();require(context->scene().nodes.size()==before.at("nodes").size()+3,"Production operation inserts scene nodes");
 {auto bad=data;bad["nodes"][0]["parent"]="left";write(source,bad);require(!session.edit("scene.import",{{"path",source.u8string()}}),"Production operation rejects cyclic parent chain");require(context->documentContent()==after,"Rejected parse leaves complete document unchanged");write(source,data);}
 run(session,"history.undo");require(context->documentContent()==before,"Single transaction undo restores source mapping and nodes");run(session,"history.redo");require(context->documentContent()==after,"Redo restores import");
 const auto count=context->scene().nodes.size();run(session,"scene.import",{{"path",source.u8string()}});require(context->scene().nodes.size()==count,"Reimport preserves identity without duplicates");
 for(std::size_t i=0;i<context->scene().nodes.size();++i)if(context->scene().nodes[i].id==left)context->selectNode(i);
 context->setGizmoTranslation({9,0,0});const auto edited=context->documentContent();require(!session.edit("scene.import",{{"path",source.u8string()}}),"Reimport detects local edits");require(context->documentContent()==edited,"Conflict leaves document untouched");
 run(session,"history.undo");data["nodes"][1]["localTransform"]["translation"]={0,300,0};write(source,data);run(session,"scene.import",{{"path",source.u8string()}});run(session,"document.save");
 auto reopened=EditorContext::openProject(root/"project.azureproject");EditorSession reopenedSession(reopened);require(reopened->levelDocument().contains("sceneImports"),"Source mapping persists through save reopen");run(reopenedSession,"scene.import",{{"path",source.u8string()}});
 const auto stable=reopened->documentContent();run(reopenedSession,"scene.import-start",{{"path",source.u8string()}});for(unsigned i=0;i<1000;++i){reopenedSession.pollTasks();bool running=false;for(const auto& task:reopenedSession.tasks().report())running|=task.at("state")=="running";if(!running)break;std::this_thread::sleep_for(std::chrono::milliseconds(2));}require(reopened->documentContent()==stable,"Async UI path commits equivalent proposal");require(reopenedSession.tasks().report().back().at("state")=="completed","Async task completed successfully");
 const auto task=run(reopenedSession,"scene.import-start",{{"path",source.u8string()}}).at("task");run(reopenedSession,"tasks.cancel",{{"id",task}});for(unsigned i=0;i<100;++i){reopenedSession.pollTasks();std::this_thread::sleep_for(std::chrono::milliseconds(2));}require(reopened->documentContent()==stable,"Async cancellation leaves scene unchanged");
 require(reopenedSession.tasks().report().back().at("state")=="cancelled","Task records cancellation");
 run(reopenedSession,"scene.import-start",{{"path",(root/"assets/missing.azuresceneexchange").u8string()}});for(unsigned i=0;i<100;++i){reopenedSession.pollTasks();std::this_thread::sleep_for(std::chrono::milliseconds(2));}require(reopenedSession.tasks().report().back().at("state")=="failed"&&reopened->documentContent()==stable,"Async read failure leaves document intact");
 std::filesystem::copy(root,moved,std::filesystem::copy_options::recursive);auto relocated=EditorContext::openProject(moved/"project.azureproject");EditorSession relocatedSession(relocated);run(relocatedSession,"scene.import",{{"path",(moved/"assets/room.azuresceneexchange").u8string()}});require(relocated->scene().nodes.size()==reopened->scene().nodes.size(),"Directory move keeps source identity and references");
 if(argc==3){const auto exchange=std::filesystem::path(argv[2]);std::filesystem::copy(exchange.parent_path(),root/"assets/blender-package",std::filesystem::copy_options::recursive);reopened->assets().refresh();const auto result=run(reopenedSession,"scene.import",{{"path",(root/"assets/blender-package"/exchange.filename()).u8string()}});require(result.at("resources")==1&&result.at("nodeMap").size()==5,"Real Blender export imports shared mesh, hierarchy, camera and light");run(reopenedSession,"document.save");auto realReopen=EditorContext::openProject(root/"project.azureproject");require(realReopen->scene().lights.size()==1,"Real Blender light survives reopen");std::cout<<"Real Blender scene exchange passed\n";}
 {auto closing=EditorContext::openProject(root/"project.azureproject");EditorSession closingSession(closing);run(closingSession,"scene.import-start",{{"path",source.u8string()}});}
 std::cout<<"Scene exchange parser, transactions, async import, cancellation, reimport and relocation passed\n";std::filesystem::remove_all(root);std::filesystem::remove_all(moved);return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\nEvidence: "<<root<<'\n';return 1;}}

