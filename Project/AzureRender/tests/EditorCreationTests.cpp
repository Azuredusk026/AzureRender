#include "editor/EditorSession.hpp"
#include "runtime/ComponentRegistry.hpp"
#include "assets/GltfLoader.hpp"
#include <chrono>
#include <iostream>
using namespace azurerender;
static void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(int argc,char** argv){
    const auto root=std::filesystem::temp_directory_path()/("azure-creation-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int code=0;
    try {
        require(argc==2,"Project fixture required");
        std::filesystem::copy(argv[1],root,std::filesystem::copy_options::recursive);
        auto context=EditorContext::openProject(root/"project.azureproject");EditorSession session(context);
        auto create=[&](const std::string& preset,const std::string& parent=std::string()) {
            auto result=session.edit("node.create-preset",{{"preset",preset},{"parent",parent}});
            require(bool(result),"Registered creation preset must succeed");return context->selectedNode()->id;
        };
        auto id=create("empty");require(context->selectedNode()->name=="Empty0","Empty gets a readable name");
        require(session.selection().consumeReveal(),"Creation must reveal its selected object in the hierarchy");
        require(context->componentData(id,"azure.rigid-body").is_null(),"Empty must have no implicit collider");
        auto cube=create("cube");require(context->selectedNode()->name=="Cube0"&&context->selectedNode()->visible,"Cube must be named and visible");
        require(!loadGltfAsset(context->assets().resolveReference(context->selectedNode()->resourceId).u8string()).vertices.empty(),"Created geometry must actually load");
        require(context->componentData(cube,"azure.rigid-body").at("shape")=="box","Cube must use box collision");
        create("cube");require(context->selectedNode()->name=="Cube1","Creation allocates unused numbers");
        context->undo();create("cube");require(context->selectedNode()->name=="Cube1","Undo makes the name available again");
        context->setSelectedNodeName("中文立方体");require(context->selectedNode()->id!=cube,"New object retains independent identity");
        auto sphere=create("sphere");require(context->componentData(sphere,"azure.rigid-body").at("shape")=="sphere","Sphere collision fits its shape");
        auto capsule=create("capsule");require(context->componentData(capsule,"azure.rigid-body").at("shape")=="capsule","Capsule collision fits its shape");
        create("torus");require(context->componentData(context->selectedNode()->id,"azure.rigid-body").is_null(),"Torus requires explicit collision fitting");
        create("plane");create("cylinder");create("game-ui");create("light");create("camera");
        const auto parent=create("empty");context->setGizmoTranslation({3,0,0});const auto child=create("cube",parent);
        require(context->selectedNode()->parentId==parent,"Child preset preserves requested hierarchy");
        const auto before=context->documentContent();const auto history=context->undoCount();
        require(!session.edit("node.reparent",{{"id",parent},{"parent",child}}),"Hierarchy cycles must be rejected");
        require(context->documentContent()==before&&context->undoCount()==history,"Rejected cycle preserves document and history");
        require(bool(session.edit("node.rename",{{"value","子对象"}})),"Rename uses the shared operation");
        require(context->selectedNode()->id==child,"Rename preserves stable identity");
        require(bool(session.edit("node.duplicate")),"Context action duplicates through the service");
        require(context->selectedNode()->id!=child&&context->selectedNode()->parentId==parent,"Duplicate has independent identity and preserves parent");
        require(bool(session.edit("history.undo")),"Duplicate is undoable");
        require(session.selection().consumeReveal(),"Undo requests hierarchy reveal");
        const auto afterUndo=context->documentContent();const auto afterUndoHistory=context->undoCount();
        require(!session.edit("node.create-preset",{{"preset","cube"},{"parent","missing"}}),"Invalid parent is rejected");
        require(context->documentContent()==afterUndo&&context->undoCount()==afterUndoHistory,"Failed creation protects document and history");
        context->save();auto reopened=EditorContext::openProject(root/"project.azureproject");
        require(std::any_of(reopened->scene().nodes.begin(),reopened->scene().nodes.end(),[&](const auto& node){return node.id==child&&node.parentId==parent;}),"Creation hierarchy survives save and reopen");
        std::cout<<"Real geometry, naming, colliders, hierarchy, rollback and reopen passed\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';code=1;}
    std::filesystem::remove_all(root);return code;
}
