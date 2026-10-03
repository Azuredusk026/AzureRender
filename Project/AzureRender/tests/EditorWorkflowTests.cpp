#include "editor/EditorSession.hpp"
#include "runtime/Project.hpp"
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <fstream>
using namespace azurerender;
void check(bool value, const char* reason) { if (!value) throw std::runtime_error(reason); }
int main(int argc, char** argv) {
    if (argc != 2) return 2;
    const auto root = std::filesystem::temp_directory_path()/"azure_editor_workflow";
    std::filesystem::remove_all(root);
    try {
        std::filesystem::copy(argv[1], root, std::filesystem::copy_options::recursive);
        auto context = EditorContext::openProject(root/"project.azureproject");
        const auto original = context->scene().nodes.size();
        const auto resource = context->importAsset(std::filesystem::path(argv[1]).parent_path()/"test_model.gltf");
        context->placeResource(resource);
        context->setSelectedNodeName("Imported");
        context->setGizmoTranslation({2,3,4});
        context->duplicateSelection();
        check(context->scene().nodes.size()==original+2, "Placement and duplicate must create nodes");
        check(context->undo() && context->scene().nodes.size()==original+1, "Undo must remove duplicate");
        check(context->redo() && context->scene().nodes.size()==original+2, "Redo must restore duplicate");
        context->selectNodes({original,original+1});context->deleteSelection();
        check(context->scene().nodes.size()==original, "Multi-delete must remove every selected node");
        check(context->undo() && context->scene().nodes.size()==original+2, "Multi-delete must be one history action");
        context->selectNode(1);
        context->setComponentField("azure.character","speed",2.5);
        context->renderSettings().grade.exposureEv=1.25F;
        context->save();
        auto reopened = EditorContext::openProject(root/"project.azureproject");
        check(reopened->scene().nodes.size()==original+2 && reopened->componentData("hero","azure.character").at("speed")==2.5, "Saved component and placement must reopen");
        check(reopened->renderSettings().grade.exposureEv==1.25F,"Project rendering edits must survive save");
        EditorSession session(reopened);
        reopened->setGizmoTranslation({0,2,0});
        const auto edited = reopened->scene().nodes[1].translation;
        check(session.execute(EditorCommand::Play), "Play must start a separate World");
        session.game()->input().key(68,true);session.advance(1.0/60.0);
        check(reopened->scene().nodes[1].translation==edited, "Play must preserve edit state");
        check(session.execute(EditorCommand::Pause), "Pause must be available while running");
        const auto steps=session.game()->steps();session.advance(1.0/60.0);
        check(session.game()->steps()==steps, "Pause must stop fixed steps");
        check(session.execute(EditorCommand::Step), "Single step must be available while paused");session.advance(1.0/60.0);
        check(session.game()->steps()==steps+1, "Single step must advance exactly once");
        check(session.execute(EditorCommand::Stop) && !session.playing() && reopened->scene().nodes[1].translation==edited, "Stop must restore editing");
        check(reopened->canUndo(), "Stop must preserve edit history");
        bool failed=false;try{context->importAsset(root/"missing.gltf");}catch(const std::exception&){failed=true;}
        check(failed && context->scene().nodes.size()==original+2, "Failed import must preserve the scene");
        context->startImport(std::filesystem::path(argv[1]).parent_path()/"test_model.gltf");context->cancelImport();
        while(context->importing()){try{context->pollImport();}catch(const std::exception&){}std::this_thread::yield();}
        check(context->scene().nodes.size()==original+2,"Cancelled import must preserve nodes");
        nlohmann::json prefab={{"schemaVersion",1},{"id","test-prefab"},{"resources",nlohmann::json::array({{{"id","mesh"},{"asset","engine:/assets_public/test_model.gltf"}}})},{"nodes",nlohmann::json::array({{{"id","root"},{"resourceId","mesh"}}})},{"lights",nlohmann::json::array({{{"id","lamp"},{"nodeId","root"}}})}};
        std::ofstream(root/"assets/test.azureprefab")<<prefab.dump();
        auto document=context->levelDocument();document["prefabs"]=nlohmann::json::array({{{"instance","test"},{"asset","assets:/test.azureprefab"}}});std::ofstream(root/"assets/courtyard.azurelevel")<<document.dump();
        auto instances=EditorContext::openProject(root/"project.azureproject");instances->selectNode(instances->scene().nodes.size()-1);instances->setSelectedNodeName("Prefab Override");instances->save();
        instances=EditorContext::openProject(root/"project.azureproject");check(instances->scene().nodes.back().name=="Prefab Override","Prefab overrides and lights must reopen");
        const auto instanceIndex=instances->scene().nodes.size()-1;instances->selectNode(instanceIndex);instances->duplicateSelection();instances->selectNode(instanceIndex);instances->deleteSelection();instances->save();
        check(EditorContext::openProject(root/"project.azureproject")->scene().nodes.back().prefabSource.empty(),"Detached duplicate must survive original prefab deletion");
        Project::create(root/"legacy","Legacy");auto legacy=EditorContext::openProject(root/"legacy/project.azureproject");legacy->setSelectedNodeName("Legacy Object");legacy->save();
        check(EditorContext::openProject(root/"legacy/project.azureproject")->scene().nodes[0].name=="Legacy Object","Legacy project must remain editable");
        std::cout << "Project import, placement, editing, history, save/reopen and isolated play passed\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';std::filesystem::remove_all(root);return 1;}
    std::filesystem::remove_all(root);
}
