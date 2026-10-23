#include "editor/EditorContext.hpp"
#include "runtime/RuntimeLifecycle.hpp"
#include "scene/SceneDescription.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
using namespace azurerender;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class F> void rejects(F action){bool bad=false;try{action();}catch(const std::exception&){bad=true;}require(bad,"Invalid object shadow document accepted");}
}
int main(){
    auto root=std::filesystem::temp_directory_path()/("azure_object_shadow_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try{
        std::filesystem::create_directories(root);
        auto& registry=runtimeComponentRegistry();auto envelope=registry.defaults("azure.object-shadow");
        require(envelope.at("version")==1&&envelope.at("data").at("atlasPage")==8,"Registered defaults or version missing");
        for(const auto& change:std::vector<std::pair<std::string,Json>>{{"halfExtents",{0,1,1}},{"atlasPage",9},{"atlasPage",1.5},{"filterRadiusX",-1},{"normalBias",101},{"unknown",0}}){
            auto bad=envelope;bad["data"][change.first]=change.second;rejects([&]{registry.validate("azure.object-shadow",bad);});
        }
        auto bad=envelope;bad["version"]=2;rejects([&]{registry.validate("azure.object-shadow",bad);});
        SceneDocument document;SceneNode node;node.id="object";node.objectShadow=ecs::ObjectShadowComponent{};
        node.objectShadow->center={.125F,.25F,-.5F};node.objectShadow->atlasPage=6;node.objectShadow->normalBias=.00933641195F;document.nodes.push_back(node);
        const auto path=root/"shadow.azscene";document.save(path);auto reopened=SceneDocument::load(path);
        require(reopened.nodes[0].objectShadow&&reopened.nodes[0].objectShadow->normalBias==node.objectShadow->normalBias,"Saved shadow precision or presence lost");
        std::string legacy;{std::ifstream file(path);legacy.assign(std::istreambuf_iterator<char>(file),{});}
        const auto field=legacy.find("objectShadow ");require(field!=std::string::npos,"Version thirteen must serialize an explicit optional shadow field");
        legacy.erase(field,legacy.find('\n',field)-field+1);const auto header=legacy.find("schemaVersion 13");
        require(header!=std::string::npos,"Unexpected current scene version");legacy.replace(header,16,"schemaVersion 12");
        {std::ofstream file(root/"legacy.azscene");file<<legacy;}
        require(!SceneDocument::load(root/"legacy.azscene").nodes[0].objectShadow,"Version twelve must load with an empty object shadow");
        auto minimum=document;minimum.nodes[0].objectShadow->halfExtents={.0001F,.0001F,.0001F};
        minimum.save(root/"minimum.azscene");require(SceneDocument::load(root/"minimum.azscene").nodes[0].objectShadow->halfExtents==minimum.nodes[0].objectShadow->halfExtents,"Minimum float extents failed their save/reopen boundary");
        require(reopened.renderDescription().nodes[0].objectShadow->atlasPage==6,"Typed render description lost the object shadow");
        auto snapshot=reopened.renderDescription();reopened.nodes[0].objectShadow->atlasPage=2;
        require(snapshot.nodes[0].objectShadow->atlasPage==6,"Render snapshot retained mutable component storage");
        RuntimeLifecycle runtime;runtime.loadScene(document);const auto entity=runtime.entity("object");
        auto* shadow=runtime.world().tryGet<ecs::ObjectShadowComponent>(entity);require(shadow&&shadow->atlasPage==6,"Runtime failed to install document shadow");
        shadow->atlasPage=3;require(runtime.snapshotScene().nodes[0].objectShadow->atlasPage==3,"Runtime shadow edit did not reach its snapshot");
        runtime.world().removeComponent<ecs::ObjectShadowComponent>(entity);require(!runtime.snapshotScene().nodes[0].objectShadow,"Removed runtime shadow retained stale data");
        runtime.start();SceneNode spawned=node;spawned.id="spawned";runtime.deferSpawn(spawned);runtime.beginFrame(0);
        require(runtime.world().tryGet<ecs::ObjectShadowComponent>(runtime.entity("spawned"))!=nullptr,"Spawn failed to install the typed object shadow");
        runtime.stop();
        Project::create(root/"project","Object shadow inspection");auto project=root/"project/project.azureproject";
        Json data;{std::ifstream file(project);file>>data;}data["startupScene"]="assets:/inspect.azurelevel";{std::ofstream file(project);file<<data.dump(2);}
        auto level=root/"project/assets/inspect.azurelevel";
        {std::ofstream file(level);file<<Json{{"schemaVersion",1},{"id","inspection"},{"resources",Json::array()},{"nodes",Json::array({{{"id","tool"},{"components",Json::object()}}})}}.dump(2);}
        auto editor=EditorContext::openProject(project);editor->selectNode(0);editor->addGameplayComponent("azure.object-shadow");
        editor->setComponentFields("azure.object-shadow",{{"atlasPage",6},{"halfExtents",{.8,1.25,1.24}}});
        require(editor->scene().renderDescription().nodes[0].objectShadow->atlasPage==6,"Shared editor operation failed to update render data");
        editor->save();editor=EditorContext::openProject(project);editor->selectNode(0);
        require(editor->scene().nodes[0].objectShadow->halfExtents[1]==1.25F,"Canonical level save/reopen lost typed shadow fields");
        editor->removeGameplayComponent("azure.object-shadow");require(!editor->scene().nodes[0].objectShadow,"Editor removal retained the object shadow");
        require(editor->undo()&&editor->scene().nodes[0].objectShadow.has_value(),"Undo failed to restore object shadow");
        require(editor->redo()&&!editor->scene().nodes[0].objectShadow,"Redo failed to remove object shadow");
        std::filesystem::remove_all(root);std::cout<<"Object shadow metadata, documents, snapshots, runtime and editor roundtrip passed\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';std::filesystem::remove_all(root);return 1;}
}
