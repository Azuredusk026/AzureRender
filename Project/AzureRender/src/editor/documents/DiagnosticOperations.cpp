#include "editor/EditorSession.hpp"
namespace azurerender {
void EditorSession::collectDiagnostics(){
    if(auto* runtime=scripts()){
        std::set<std::string> live;
        for(auto diagnostic:runtime->diagnostics()){
            live.insert(diagnostic.id);if(runtimeDiagnosticIds_.count(diagnostic.id))continue;
            diagnostic.document=context_->documentId();diagnostics_.record(std::move(diagnostic));
        }runtimeDiagnosticIds_=std::move(live);
    }else runtimeDiagnosticIds_.clear();
    if(developerServices_.report){const auto report=developerServices_.report();const auto message=report.value("shaderDiagnostic",std::string());
        if(!message.empty()&&message!=shaderDiagnostic_){StructuredDiagnostic diagnostic;diagnostic.source="shader.compile";diagnostic.code="CompilationFailed";diagnostic.message=message;diagnostic.document=context_->documentId();diagnostics_.record(std::move(diagnostic));}shaderDiagnostic_=message;}
}
void EditorSession::revealDiagnostic(const std::string& id){
    const auto diagnostic=diagnostics_.at(id);
    if(diagnostic.document.empty())throw EditRejection("Diagnostic has no document location");
    const auto& document=documents_.at(diagnostic.document);
    if(!diagnostic.node.empty()&&std::none_of(document.context->scene().nodes.begin(),document.context->scene().nodes.end(),[&](const auto& node){return node.id==diagnostic.node;}))throw EditRejection("Diagnostic node is no longer present");
    if(!diagnostic.asset.empty()){static_cast<void>(document.context->assets().resolveReference(diagnostic.asset));assetReveal_=diagnostic.asset;}
    requestDocumentActivation(diagnostic.document);diagnosticNode_=diagnostic.node;
}
void registerDiagnosticOperations(EditRegistry& registry,EditorSession& session){
    using Json=nlohmann::json;const Json text={{"type","string"}};
    registry.add({"diagnostic.query",1,{{"type","object"},{"properties",{{"source",text},{"severity",text},{"code",text},{"document",text},{"text",text}}}},false,false,false},[&session](EditorContext&,const Json& filter)->Json{return session.diagnostics().query(filter);});
    registry.add({"diagnostic.reveal",1,{{"type","object"},{"properties",{{"id",text}}},{"required",{"id"}}},false,false,true},[&session](EditorContext&,const Json& a)->Json{session.revealDiagnostic(a.at("id"));return {{"pending",true}};});
}
}
