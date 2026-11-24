#include "editor/EditorSession.hpp"
#include <iostream>

using namespace azurerender;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void verify(SceneDocument scene,const std::string& name){
    auto context=std::make_shared<EditorContext>(scene,std::filesystem::temp_directory_path()/(name+".azscene"));
    EditorSession session(context);
    session.selection().set({});
    const auto count=context->scene().nodes.size();
    check(static_cast<bool>(session.edit("node.add")),"Add must create a root without a selection");
    check(context->scene().nodes.size()==count+1,"Exactly one root must be created");
    check(context->selectedNode()&&context->selectedNode()->parentId.empty(),"Created root must be selected");
    const auto parent=context->selectedNode()->id;
    check(static_cast<bool>(session.edit("node.add")),"Add must create a child of the active selection");
    check(context->scene().nodes.size()==count+2,"Exactly one child must be created");
    check(context->selectedNode()&&context->selectedNode()->parentId==parent,"Child must inherit the selected parent");
    check(context->undoCount()==2,"Each Add must form one undo operation");
    check(static_cast<bool>(session.edit("history.undo")),"Child addition must undo");
    check(context->scene().nodes.size()==count+1,"Undo must remove only the child");
    check(static_cast<bool>(session.edit("history.undo")),"Root addition must undo");
    check(context->scene().nodes.size()==count,"Undo must restore the original scene");
    check(static_cast<bool>(session.edit("history.redo")),"Root addition must redo");
    check(static_cast<bool>(session.edit("history.redo")),"Child addition must redo");
    check(context->scene().nodes.size()==count+2,"Redo must restore both additions");
    const auto before=context->documentContent();const auto history=context->undoCount();
    bool rejected=false;
    try{context->createNode("invalid-parent","missing-parent");}catch(const std::invalid_argument&){rejected=true;}
    check(rejected&&context->documentContent()==before&&context->undoCount()==history,"Invalid parent must preserve content and history");
}
}
int main(){try{
    SceneDocument blank;blank.sceneId="blank";verify(blank,"blank-addition");
    SceneDocument populated;populated.sceneId="inspection";
    SceneNode node;node.id="existing";node.name="Existing";populated.nodes.push_back(node);
    verify(populated,"inspection-addition");
    std::cout<<"Contextual addition, empty selection and undo/redo passed\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
