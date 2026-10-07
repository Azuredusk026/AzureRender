#include "editor/EditorSession.hpp"
#include "editor/documents/DocumentActionGuard.hpp"
#include <iostream>
int main(){
    using namespace azurerender;
    auto context=std::make_shared<EditorContext>(SceneDocument::fromAsset("mesh.gltf"),std::filesystem::temp_directory_path()/"azure-guard-missing"/"scene.azscene");
    context->setSelectedNodeName("Unsaved");
    const auto content=context->documentContent();
    DocumentActionGuard guard([&]{return context->dirty();},[&]{context->save();});
    if(guard.request(DocumentAction::Close)!=DocumentActionState::AwaitingDecision){std::cerr<<"Dirty close needs a decision\n";return 1;}
    if(guard.resolve(DocumentDecision::Save)!=DocumentActionState::Failed || guard.diagnostic().empty())return 2;
    if(context->documentContent()!=content || !context->dirty())return 3;
    if(guard.resolve(DocumentDecision::Cancel)!=DocumentActionState::Idle)return 4;
    guard.request(DocumentAction::Reload);
    if(guard.resolve(DocumentDecision::Discard)!=DocumentActionState::Ready || guard.action()!=DocumentAction::Reload)return 5;
    if(context->documentContent()!=content)return 6;
    return 0;
}
