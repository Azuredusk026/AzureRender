#include "editor/EditorSession.hpp"
#include "editor/commands/EditService.hpp"
#include <filesystem>
#include <iostream>
#include <stdexcept>
using namespace azurerender;
namespace {
void require(bool value,const char* message) { if(!value)throw std::runtime_error(message); }
EditRequest request(EditService& edits,const std::string& command,nlohmann::json parameters,
                    std::string merge={}) {
    static unsigned sequence=0;
    return {"test-"+std::to_string(++sequence),command,std::move(parameters),edits.version(),std::move(merge)};
}
void rollbackAndHistory(EditorSession& session) {
    auto& edits=session.edits();auto& context=session.context();
    const auto before=context.documentContent();const auto selected=context.selectedNodes();
    const auto dirty=context.dirty();const auto undo=context.undoCount();const auto redo=context.redoCount();
    const auto version=edits.version();
    auto result=edits.executeBatch({request(edits,"node.create",{{"id","candidate"}}),
                                   request(edits,"node.create",{{"id","candidate"}})});
    require(result.status==EditStatus::Failed,"A conflicting second operation must fail the transaction");
    require(context.documentContent()==before&&context.selectedNodes()==selected&&context.dirty()==dirty,
            "A failed transaction must retain document, selection and dirty state");
    require(context.undoCount()==undo&&context.redoCount()==redo&&edits.version()==version,
            "A failed transaction must retain history and version");
    result=edits.executeBatch({request(edits,"node.create",{{"id","first"}}),
                              request(edits,"node.rename",{{"value","Batch object"}})});
    require(result.status==EditStatus::Applied&&context.scene().nodes.back().name=="Batch object",
            "A valid transaction must commit the complete candidate");
    require(context.undoCount()==undo+1,"A batch must form exactly one undo unit");
    const auto committed=context.documentContent();
    require(edits.undo().status==EditStatus::Applied&&context.documentContent()==before,
            "One undo must restore the complete batch");
    require(edits.redo().status==EditStatus::Applied&&context.documentContent()==committed,
            "One redo must restore all batch operations");
    const auto old=request(edits,"node.rename",{{"value","outdated"}});
    require(edits.undo().status==EditStatus::Applied,"Undo must remain available");
    require(edits.redo().status==EditStatus::Applied,"Redo must remain available");
    require(edits.execute(old).status==EditStatus::Stale,
            "Undo and redo must invalidate a proposal even when content is restored");
    result=edits.executeBatch({request(edits,"node.rename",{{"value","unsaved"}}),
                              request(edits,"document.save",nlohmann::json::object())});
    require(result.status==EditStatus::Rejected&&context.documentContent()==committed,
            "External side effects must be rejected before a transaction starts");
}
void mergeAndStale(EditorSession& session) {
    auto& edits=session.edits();auto& context=session.context();
    const auto before=context.documentContent();const auto undo=context.undoCount();
    for(int value=1;value<=3;++value) {
        auto result=edits.execute(request(edits,"node.transform",{{"translation",{value,0,0}}},"drag-1"));
        require(result.status==EditStatus::Applied,"Continuous transform edits must apply");
    }
    require(context.undoCount()==undo+1,"A continuous drag must merge into one undo unit");
    require(edits.undo().status==EditStatus::Applied&&context.documentContent()==before,
            "Merged undo must restore the drag start");
    require(edits.redo().status==EditStatus::Applied&&context.selectedNode()->translation[0]==3,
            "Merged redo must restore the drag end");
    const auto proposal=request(edits,"node.rename",{{"value","late"}});
    require(edits.execute(request(edits,"document.save",nlohmann::json::object())).status==EditStatus::Applied,
            "Save must use the production operation");
    require(edits.execute(proposal).status==EditStatus::Stale,"Save must invalidate prior proposals");
    const auto reloadProposal=request(edits,"node.rename",{{"value","late-reload"}});
    require(edits.execute(request(edits,"document.reload",nlohmann::json::object())).status==EditStatus::Applied,
            "Reload must use the production operation");
    require(edits.execute(reloadProposal).status==EditStatus::Stale,"Reload must invalidate prior proposals");
    auto replacement=std::make_shared<EditorContext>(context.scene(),context.scenePath());
    EditorSession other(replacement);
    require(other.edits().execute(request(edits,"node.rename",{{"value","cross-document"}})).status==EditStatus::Stale,
            "A proposal must not apply to a replacement document");
    const auto selectedProposal=request(edits,"node.rename",{{"value","wrong-target"}});
    context.selectNode(1);
    require(edits.execute(selectedProposal).status==EditStatus::Stale,
            "A target selection change must invalidate an implicit-target proposal");
}
}
int main(int argc,char** argv) {
    if(argc!=2)return 2;
    auto path=std::filesystem::temp_directory_path()/"azure-edit-transaction.azscene";
    const auto projectPath=std::filesystem::temp_directory_path()/"azure-edit-preview-project";
    try {
        auto document=SceneDocument::fromAsset("test.gltf");
        auto context=std::make_shared<EditorContext>(document,path);EditorSession session(context);
        rollbackAndHistory(session);mergeAndStale(session);
        std::filesystem::remove_all(projectPath);
        std::filesystem::copy(argv[1],projectPath,std::filesystem::copy_options::recursive);
        {
        auto project=EditorContext::openProject(projectPath/"project.azureproject");EditorSession preview(project);
        require(static_cast<bool>(preview.edit("node.select",{{"id","hero"}})),"A project node must be selectable");
        require(static_cast<bool>(preview.edit("animation.preview",{{"state","idle"},{"time",.1}}))&&project->animationPreview(),
                "The registered animation preview must produce a pose");
        require(static_cast<bool>(preview.edit("node.select",{{"index",0}}))&&!project->animationPreview(),
                "Changing selection must release the old node's animation preview");
        require(preview.startBuild(projectPath/"missing-install",projectPath/"output"),"Build must start asynchronously");
        const auto buildDocument=project->documentContent();
        require(preview.edit("node.create",{{"id","during-build"}}).status==EditStatus::Rejected&&project->documentContent()==buildDocument,
                "Document edits must be rejected during a build");
        }
        std::filesystem::remove_all(projectPath);
        std::filesystem::remove(path);std::cout<<"Edit transaction, complete rollback, history and stale rejection passed\n";
    }catch(const std::exception& error) {
        std::filesystem::remove_all(projectPath);
        std::filesystem::remove(path);std::cerr<<error.what()<<'\n';return 1;
    }
}
