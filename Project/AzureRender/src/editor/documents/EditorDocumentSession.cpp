#include "editor/EditorSession.hpp"
namespace azurerender {
void EditorSession::retainDocument(){documents_.add({context_,edits_,selection_,gizmo_,viewportView_});}
std::string EditorSession::openDocument(const std::string& reference){
    if(!pendingDocument_.empty()||!closingDocument_.empty())throw EditRejection("Finish the queued document action first");
    std::filesystem::path path;
    if(context_->isProject()){
        path=reference.find(":/")!=std::string::npos||context_->assets().records().count(reference)?context_->assets().resolveReference(reference):std::filesystem::u8path(reference);
        bool mounted=false;for(const auto& [name,mount]:context_->project().mounts){const auto relative=std::filesystem::weakly_canonical(path).lexically_relative(std::filesystem::weakly_canonical(mount));if(!relative.empty()&&*relative.begin()!="..")mounted=true;}
        if(!mounted)throw EditRejection("Scene document must belong to a project mount");
    }else path=std::filesystem::u8path(reference);
    if(path.extension()!=".azurelevel"&&path.extension()!=".azscene")throw EditRejection("Open a scene or level document");
    auto id=documents_.find(path);
    if(id.empty()){
        auto context=context_->isProject()?EditorContext::openProject(context_->project().file,path):std::make_shared<EditorContext>(SceneDocument::load(path),path);
        auto edits=std::make_shared<EditService>(*context,editorOperations(*this),[this](const EditDescriptor& descriptor){return editEnabled(descriptor);});
        auto selection=std::make_shared<SelectionService>(*context,*edits);auto gizmo=std::make_shared<GizmoController>(*context);
        id=documents_.add({context,edits,selection,gizmo,{}});
    }
    requestDocumentActivation(id);return id;
}
void EditorSession::requestDocumentActivation(const std::string& id){
    documents_.at(id);
    if(!pendingDocument_.empty()||!closingDocument_.empty())throw EditRejection("Finish the queued document action first");
    if(context_->importing()||creatingProject_||pendingProject_||documentGuard_->state()!=DocumentActionState::Idle||(proposals_&&proposals_->state()==ProposalState::Generating))throw EditRejection("Finish the active document task first");
    pendingDocument_=id;
}
void EditorSession::requestDocumentClose(const std::string& id,const std::string& decision){
    const auto& document=documents_.at(id);
    if(decision=="cancel")return;
    if(decision!="ask"&&decision!="save"&&decision!="discard")throw EditRejection("Unknown document close decision");
    if(documents_.size()==1)throw EditRejection("Keep one scene document open");
    if(document.context->dirty()&&decision=="ask")throw EditRejection("Choose Save, Discard or Cancel for the dirty document");
    requestDocumentActivation(context_->documentId());
    if(decision=="save")documents_.save(id);
    closingDocument_=id;
}
void EditorSession::activateDocument(const std::string& id){
    if(id==context_->documentId())return;
    auto& next=documents_.at(id);
    std::unique_ptr<ProposalController> proposal;if(model_)proposal=std::make_unique<ProposalController>(*next.context,*next.edits,generators_,*model_);
    documents_.at(context_->documentId()).view=viewportView_;
    references_.cancel();content_.reset();contentDatabase_=nullptr;contentDatabaseVersion_=0;contentDirectories_.reset();materials_.clear();
    context_->closeEditMerge();context_->detachRenderSettings();proposals_.reset();
    context_=next.context;edits_=next.edits;selection_=next.selection;gizmo_=next.gizmo;proposals_=std::move(proposal);
    viewportView_=next.view;viewportViewChanged_=true;runtimeReset_=true;assetReloadRequested_=true;
}
void EditorSession::pollDocuments(){
    if(!closingDocument_.empty()){
        if(closingDocument_==context_->documentId())for(const auto& document:documents_.describe(closingDocument_))if(document.at("id")!=closingDocument_){activateDocument(document.at("id"));break;}
        documents_.erase(closingDocument_);closingDocument_.clear();pendingDocument_.clear();
    }else if(!pendingDocument_.empty()){auto id=std::move(pendingDocument_);pendingDocument_.clear();activateDocument(id);}
    if(!diagnosticNode_.empty()){auto node=std::move(diagnosticNode_);diagnosticNode_.clear();selection_->set({node});}
}
}
