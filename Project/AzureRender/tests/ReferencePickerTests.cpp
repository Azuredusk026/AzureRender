#include "editor/EditorSession.hpp"
#include "editor/properties/ReferencePickerService.hpp"
#include <chrono>
#include <iostream>
using namespace azurerender;
int main(){try{
    auto require=[](bool ok,const char* message){if(!ok)throw std::runtime_error(message);};
    SceneDocument scene;scene.sceneId="references";
    for(const auto* id:{"a","b","target"}){SceneNode node;node.id=id;node.name=id;scene.nodes.push_back(node);}
    auto context=std::make_shared<EditorContext>(scene,std::filesystem::temp_directory_path()/("azure-reference-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".azscene"));
    EditorSession session(context);
    for(const auto* id:{"a","b"}){session.selection().set({id});require(static_cast<bool>(session.edit("component.add",{{"type","azure.third-person-camera"}})),"Add reference owner");}
    session.selection().set({"a","b"});const auto undo=context->undoCount();
    require(static_cast<bool>(session.edit("reference.begin",{{"nodes",{"a","b"}},{"type","azure.third-person-camera"},{"field","target"}})),"Begin batch reference request");
    require(session.references().active(),"Reference request must capture selection mode");
    require(!session.edit("reference.deliver",{{"kind","asset"},{"id","target"}}),"Wrong reference kind must be rejected");
    require(session.references().active(),"Invalid candidate must leave the picker usable");
    require(static_cast<bool>(session.edit("reference.deliver",{{"kind","node"},{"id","target"}})),"Valid candidate must use the editing service");
    require(context->componentData("a","azure.third-person-camera").at("target")=="target"&&context->componentData("b","azure.third-person-camera").at("target")=="target","Reference must apply to all captured owners");
    require(!session.references().active()&&context->undoCount()==undo+1,"Reference delivery must create a single undo");
    require(session.selection().selected()==std::vector<std::string>{"a","b"},"Picking must preserve selection");
    session.edit("history.undo");require(context->componentData("a","azure.third-person-camera").at("target")=="a","Undo must restore original references");
    session.edit("reference.begin",{{"nodes",{"a"}},{"type","azure.third-person-camera"},{"field","target"}});
    session.edit("node.rename",{{"value","Changed"}});
    require(!session.edit("reference.deliver",{{"kind","node"},{"id","target"}}),"Stale reference request must not write");
    session.edit("reference.cancel");require(!session.references().active(),"Cancel must end reference picking");
    require(static_cast<bool>(session.edit("reference.reveal",{{"kind","node"},{"id","target"}})),"References must reveal their target in the hierarchy");
    require(session.selection().active()=="target"&&!session.consumeFrameSelection(),"Reveal must select without moving the camera");
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
