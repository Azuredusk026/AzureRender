#include "editor/EditorSession.hpp"
#include <iostream>
using namespace azurerender;
void check(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
int main(){try{
    SceneDocument scene;scene.sceneId="selection";
    for(const auto* id:{"a","b","c","d"}){SceneNode node;node.id=id;node.name=id;scene.nodes.push_back(node);}
    auto context=std::make_shared<EditorContext>(scene,std::filesystem::temp_directory_path()/"selection.azscene");EditorSession session(context);
    session.selection().set({"d","a"});
    check(context->selectedNode()->id=="a","Active object must follow the most recent identity, independently of storage order");
    const auto click=[&](const char* id,bool ctrl=false,bool shift=false){return session.edit("selection.click",{{"id",id},{"ctrl",ctrl},{"shift",shift},{"visible",{"d","b","a"}}});};
    check(static_cast<bool>(click("d")),"Shared click operation must be registered");
    check(static_cast<bool>(click("a",false,true)),"Range selection must be accepted");
    check(session.selection().selected()==std::vector<std::string>({"d","b","a"}),"Range selection follows filtered visible order");
    check(static_cast<bool>(click("b",true)),"Toggle selection must be accepted");
    check(session.selection().selected()==std::vector<std::string>({"d","a"}),"Ctrl toggles one identity without clearing others");
    check(static_cast<bool>(click("",true))&&session.selection().selected().size()==2,"Ctrl blank preserves selection");
    check(static_cast<bool>(click(""))&&session.selection().selected().empty(),"Blank click clears selection");
    check(!click("missing"),"Invalid click preserves selection and rejects identity");
    click("a");click("d",false,true);
    check(session.selection().active()=="d"&&session.selection().selected().size()==3,"Backward range keeps the clicked object active");
    check(!session.edit("viewport.gizmo-options",{{"space","local"},{"pivot","invalid"}}),"Invalid gizmo options must be rejected");
    check(context->gizmoSpace()==EditorContext::GizmoSpace::World,"Rejected options preserve the coordinate space");
    check(!context->dirty()&&context->undoCount()==0,"Selection must preserve document content and history");
    std::cout<<"Active identity, filtered range and shared click semantics passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
