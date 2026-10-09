#include "editor/EditorSession.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
using namespace azurerender;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(int argc,char** argv){if(argc!=2)return 2;try{
    const auto folder=std::filesystem::temp_directory_path()/("azure-documents-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(folder);
    SceneDocument first;first.sceneId="first";SceneNode a;a.id="a";a.name="A";first.nodes.push_back(a);first.save(folder/"first.azscene");
    SceneDocument second=first;second.sceneId="second";second.nodes[0].id="b";second.nodes[0].name="B";second.save(folder/"second.azscene");
    auto original=std::make_shared<EditorContext>(first,folder/"first.azscene");EditorSession session(original);
    const auto firstId=original->documentVersion().documentId;
    require(static_cast<bool>(session.edit("node.rename",{{"value","First modified"}})),"Edit first document");
    const auto firstVersion=original->documentVersion();
    require(static_cast<bool>(session.edit("document.open",{{"path",(folder/"second.azscene").u8string()}})),"Open another scene as a retained document");
    session.pollTasks();require(session.context().scene().sceneId=="second","Activate opened document");
    require(!session.context().dirty()&&session.context().undoCount()==0,"New document has independent history");
    const auto secondId=session.context().documentVersion().documentId;
    require(static_cast<bool>(session.edit("node.rename",{{"value","Second modified"}})),"Edit second document");
    require(static_cast<bool>(session.edit("document.activate",{{"id",firstId}})),"Activate first document");session.pollTasks();
    require(&session.context()==original.get()&&original->documentVersion()==firstVersion,"Activation retains context and version");
    require(session.execute(EditorCommand::Undo),"Undo first document independently");
    require(original->selectedNode()->name=="A","First history restores only first scene");
    require(static_cast<bool>(session.edit("document.activate",{{"id",secondId}})),"Return to second document");session.pollTasks();
    require(session.context().selectedNode()->name=="Second modified"&&session.context().undoCount()==1,"Second document retains edits and selection");
    require(!session.edit("document.close",{{"id",secondId}}),"Dirty close requires an explicit decision");
    require(session.context().dirty(),"Rejected close preserves document");
    require(static_cast<bool>(session.edit("document.close",{{"id",secondId},{"decision","cancel"}})),"Cancel close");session.pollTasks();
    require(session.context().documentVersion().documentId==secondId,"Cancel retains active document");
    require(static_cast<bool>(session.edit("document.save",{{"id",secondId}})),"Save requested document");
    require(!session.context().dirty()&&SceneDocument::load(folder/"second.azscene").nodes[0].name=="Second modified","Save affects requested path");
    require(static_cast<bool>(session.edit("document.close",{{"id",secondId}})),"Close saved document");session.pollTasks();
    require(session.context().documentVersion().documentId==firstId,"Closing active document activates another retained context");
    require(!session.edit("document.activate",{{"id","missing"}}),"Unknown document is rejected");
    require(!session.edit("document.open",{{"path",(folder/"missing.azscene").u8string()}}),"Missing file preserves current document");
    require(session.execute(EditorCommand::Simulate),"Start simulation");
    require(!session.edit("document.open",{{"path",(folder/"second.azscene").u8string()}}),"Preview owns active document");
    require(session.execute(EditorCommand::Stop),"Stop simulation");
    std::filesystem::copy(argv[1],folder/"project",std::filesystem::copy_options::recursive);
    auto projectContext=EditorContext::openProject(folder/"project/project.azureproject");EditorSession projectSession(projectContext);
    auto portable=projectContext->scene();portable.sceneId="secondary-resource-scene";
    for(auto& resource:portable.resources)resource.path="assets:/cube.gltf";
    portable.save(folder/"project/assets/secondary.azscene");projectContext->assets().refresh();
    require(static_cast<bool>(projectSession.edit("document.open",{{"path","assets:/secondary.azscene"}})),"Open project scene with virtual resource references");projectSession.pollTasks();
    require(projectSession.context().scene().sceneId==portable.sceneId&&std::filesystem::is_regular_file(projectSession.context().scene().resources.front().path),"Each project document resolves its resource mount");
    require(projectSession.execute(EditorCommand::Reload),"Reload current project scene");
    require(projectSession.context().scene().sceneId==portable.sceneId&&projectSession.context().scenePath().filename()=="secondary.azscene"&&std::filesystem::is_regular_file(projectSession.context().scene().resources.front().path),"Reload owns the active scene path and preserves resource resolution");
    std::filesystem::remove_all(folder);std::cout<<"Independent retained document workflows passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
