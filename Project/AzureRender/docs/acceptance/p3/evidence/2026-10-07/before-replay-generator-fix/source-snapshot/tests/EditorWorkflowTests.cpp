#include "editor/EditorSession.hpp"
#include "runtime/Project.hpp"
#include "editor/GameplayDebugGeometry.hpp"
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
        ecs::TransformComponent shape;shape.translation={2,3,4};shape.scale={2,1,3};
        const auto box=debugBox(shape,game::RigidBody{});
        check(box.size()==12 && box.front().from==std::array<float,3>{1,2.5F,2.5F},"Collision overlay must reflect world translation and scaled half extents");
        game::Character capsule;capsule.centerOffset=1;
        const auto wire=debugCapsule(shape,capsule);float maximumY=-100;
        for(const auto& edge:wire)maximumY=std::max({maximumY,edge.from[1],edge.to[1]});
        check(!wire.empty() && std::abs(maximumY-4.9F)<.001F,"Capsule overlay must use runtime radius, half height and center offset");
        std::filesystem::copy(argv[1], root, std::filesystem::copy_options::recursive);
        auto context = EditorContext::openProject(root/"project.azureproject");
        const auto original = context->scene().nodes.size();
        const auto resource = context->importAsset(std::filesystem::path(argv[1]).parent_path()/"test_model.gltf");
        check(context->importSummary().value("vertices",0)==48 && context->importSummary().at("clips").size()==1,
            "Import admission must expose real model complexity and animation clips");
        check(context->assets().records().at(resource).importGeneration.has_value(),"Imported content retains a validated source manifest");
        const auto importedPath=context->assets().resolve(resource),importedCache=context->assets().cacheFile(resource);
        const auto importedSource=context->assets().readSource(resource);
        std::ofstream(importedPath,std::ios::binary)<<importedSource<<" ";bool tamperedImport=false;
        try { context->assets().refresh(true); }catch(const std::exception& e){tamperedImport=std::string(e.what()).find("Imported output hash is stale")!=std::string::npos;}
        check(tamperedImport&&std::filesystem::exists(importedCache),"Imported output changes require a fresh source manifest");
        std::ofstream(importedPath,std::ios::binary)<<importedSource;context->assets().refresh(true);
        context->placeResource(resource,"imported-actor");
        check(context->scene().nodes.back().id=="imported-actor","Placement must retain author-selected identity for gameplay references");
        context->createNode("configured-camera");
        check(context->scene().nodes.back().id=="configured-camera" && context->scene().nodes.back().resourceId.empty(),"Empty gameplay nodes must have stable author-selected identities");
        context->undo();context->selectNode(original);
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
        const auto beforeInvalid=context->levelDocument();
        bool rejected=false;
        try { context->setComponentField("azure.script","asset","assets:/missing.lua"); }
        catch(const std::exception&) { rejected=true; }
        check(rejected && context->levelDocument()==beforeInvalid,"Missing script reference must preserve the edit candidate");
        rejected=false;
        try { context->setComponentField("azure.animator","state","missing-state"); }
        catch(const std::exception&) { rejected=true; }
        check(rejected && context->levelDocument()==beforeInvalid,"Unknown animation semantic must preserve the edit candidate");
        std::ofstream(root/"assets/bad-motion.json") << R"({"schemaVersion":1,"initial":"idle","states":[{"name":"idle","clip":99}],"transitions":[]})";
        context->assets().refresh();rejected=false;
        try { context->setComponentField("azure.animator","asset","assets:/bad-motion.json"); }
        catch(const std::exception&) { rejected=true; }
        check(rejected && context->levelDocument()==beforeInvalid,"Out-of-skeleton clip index must preserve the edit candidate");
        context->addGameplayComponent("azure.third-person-camera");
        const auto beforeCamera=context->levelDocument();rejected=false;
        try { context->setComponentField("azure.third-person-camera","target","deleted-node"); }
        catch(const std::exception&) { rejected=true; }
        check(rejected && context->levelDocument()==beforeCamera,"Camera target must resolve before committing an edit");
        rejected=false;
        try { context->addGameplayComponent("azure.rigid-body"); }
        catch(const std::exception&) { rejected=true; }
        check(rejected && context->levelDocument()==beforeCamera,"Character and rigid body must not overwrite runtime physics ownership");
        context->undo();
        context->setComponentField("azure.character","speed",2.5);
        context->renderSettings().grade.exposureEv=1.25F;
        context->save();
        auto reopened = EditorContext::openProject(root/"project.azureproject");
        check(reopened->scene().nodes.size()==original+2 && reopened->componentData("hero","azure.character").at("speed")==2.5, "Saved component and placement must reopen");
        check(reopened->renderSettings().grade.exposureEv==1.25F,"Project rendering edits must survive save");
        EditorSession session(reopened);
        const auto beforeGeneration=session.edits().version();
        const auto generated=session.edit("asset.generate",{{"generator","azure.text"},{"output","assets:/generated.azurelevel"},
            {"parameters",{{"source",R"({"schemaVersion":1,"id":"generated","sceneType":"sample","resources":[],"nodes":[]})"}}},{"license","LicenseRef-Project"}});
        check(static_cast<bool>(generated),"Generated text uses the production edit operation");
        check(generated.version!=beforeGeneration,"Generated asset changes invalidate document proposals");
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
        context->assets().refresh();
        const auto beforePrefab=context->levelDocument();
        context->placePrefab("assets:/test.azureprefab","placed");
        check(context->scene().nodes.back().id=="placed:root" && context->scene().lights.back().nodeId=="placed:root","Prefab placement must expand stable identities and lights");
        check(context->undo() && context->levelDocument()==beforePrefab,"Prefab placement must undo its serialization and resources together");
        check(context->redo() && context->scene().nodes.back().id=="placed:root","Prefab redo must restore its serialized source");
        const auto placed=context->levelDocument();rejected=false;
        try{context->placePrefab("assets:/test.azureprefab","placed");}catch(const std::exception&){rejected=true;}
        check(rejected && context->levelDocument()==placed,"Duplicate prefab candidate must retain active edit state");
        context->save();context->reload();check(context->scene().nodes.back().id=="placed:root","Placed prefab must survive save and reload");
        context->selectNode(context->scene().nodes.size()-1);context->deleteSelection();
        context->selectNode(1);const auto beforePreview=context->levelDocument();
        context->previewAnimation("run",.25);
        check(context->animationPreview().has_value() && context->animationPreview()->clip==1 && context->animationPreview()->time==.25,"Preview must use semantic clip and scrub time");
        check(context->levelDocument()==beforePreview,"Preview must preserve saved animation configuration");
        context->clearAnimationPreview();check(!context->animationPreview(),"Clearing preview must restore renderer animation ownership");
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
