#include "editor/EditorSession.hpp"
#include "scene/TransformSystem.hpp"
#include <chrono>
#include <cmath>
#include <iostream>
using namespace azurerender;
using namespace azurerender::internal;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
bool near(float a,float b){return std::abs(a-b)<1e-4F;}
Matrix4 begin(EditorSession& session,const char* space="world") {
    auto result=session.edit("viewport.gizmo-begin",{{"space",space},{"pivot","active"}});
    check(static_cast<bool>(result),"Gizmo must start through a registered production operation");
    return result.value.at("matrix").get<Matrix4>();
}
void update(EditorSession& session,const Matrix4& matrix){check(static_cast<bool>(session.edit("viewport.gizmo-update",{{"matrix",matrix}})),"Valid gizmo update rejected");}
}
int main(){
    const auto root=std::filesystem::temp_directory_path()/("azure-gizmo-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root);
    int status=0;
    try {
        SceneDocument scene;scene.sceneId="gizmo";
        SceneNode parent;parent.id="parent";parent.translation={10,0,0};parent.rotation={0,90,0};parent.scale={2,3,4};
        SceneNode child;child.id="child";child.parentId="parent";child.translation={1,0,0};
        scene.nodes={parent,child};
        auto context=std::make_shared<EditorContext>(scene,root/"scene.azscene");
        EditorSession session(context);session.selection().set({"child"});context->save();
        const auto original=context->documentContent();
        auto matrix=begin(session);check(near(matrix[12],10)&&near(matrix[14],-2),"Gizmo pivot must use world coordinates");
        check(!session.edit("viewport.gizmo-mode",{{"value",2}}),"Active drag must isolate tool switching");
        check(!session.edit("preview.play"),"Active drag must isolate play lifecycle");
        matrix[12]+=2;update(session,matrix);
        check(near(context->scene().nodes[1].translation[0],1)&&near(context->scene().nodes[1].translation[2],.5F),"World translation must pass through parent inverse");
        matrix[12]+=2;update(session,matrix);
        check(context->undoCount()==1,"Continuous updates must form one undo unit");
        check(static_cast<bool>(session.edit("viewport.gizmo-cancel")),"Cancel operation rejected");
        check(context->documentContent()==original&&!context->dirty()&&context->undoCount()==0&&context->redoCount()==0,"Cancel must restore content, dirty and history");
        matrix=begin(session,"local");
        for(unsigned axis=0;axis<3;++axis)matrix[12+axis]+=matrix[axis]*2;
        update(session,matrix);session.edit("viewport.gizmo-commit");
        check(near(context->scene().nodes[1].translation[0],2)&&near(context->scene().nodes[1].translation[2],0),"Local axis must follow object orientation");
        session.edit("history.undo");check(context->documentContent()==original&&!context->dirty(),"Undo restores the complete drag");
        const auto redoBefore=context->redoCount();
        matrix=begin(session);matrix[13]+=3;update(session,matrix);session.edit("viewport.gizmo-cancel");
        check(context->redoCount()==redoBefore,"Cancelled drag preserves pre-existing redo history");
        session.selection().set({"parent","child"});
        matrix=begin(session);matrix[12]+=3;update(session,matrix);session.edit("viewport.gizmo-commit");
        check(near(context->scene().nodes[0].translation[0],13)&&near(context->scene().nodes[1].translation[0],1),"Selected descendants must receive group delta once");
        session.edit("history.undo");session.selection().set({"child"});
        matrix=begin(session);
        const auto rotated=multiply(translation(matrix[12],matrix[13],matrix[14]),rotationZ(.785398163F));
        check(!session.edit("viewport.gizmo-update",{{"matrix",rotated}}),"Unrepresentable parent shear must be rejected");
        check(context->documentContent()==original,"Rejected transform preserves authored content");
        session.edit("viewport.gizmo-cancel");
        session.selection().set({});check(!session.edit("viewport.gizmo-begin"),"Empty selection must reject drag");
        session.selection().set({"child"});matrix=begin(session);matrix[0]=NAN;
        check(!session.edit("viewport.gizmo-update",{{"matrix",matrix}}),"Nonfinite matrices must be rejected");
        session.edit("viewport.gizmo-cancel");
        matrix=begin(session);matrix[12]+=2;update(session,matrix);
        check(session.requestDocumentAction(DocumentAction::Close),"Close request must reach the document guard");
        check(!session.gizmo().active()&&context->documentContent()==original&&!context->dirty(),"Closing cancels the incomplete drag before applying the saved-document policy");
        std::cout<<"Gizmo world/local transforms, root selection, cancellation and history passed\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';status=1;}
    std::filesystem::remove_all(root);return status;
}
