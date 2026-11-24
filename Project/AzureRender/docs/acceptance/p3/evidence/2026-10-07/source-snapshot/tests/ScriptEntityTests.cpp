#include "app/ProjectRuntimeAssembly.hpp"
#include "runtime/ScriptRuntime.hpp"
#include "runtime/ComponentCodec.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace azurerender;
void check(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
int main(){const auto root=std::filesystem::temp_directory_path()/"azure_script_entity_contract";
try{
    std::filesystem::remove_all(root);Project::create(root,"Entity queries");auto project=Project::load(root/"project.azureproject");
    const auto script=root/"assets/controller.lua";
    auto write=[&](const char* text){std::ofstream(script)<<text;};
    write("function init() local target=self:find('target'); target:set('azure.transform','translation',{2,3,4}); error('transaction rollback') end");
    AssetDatabase assets(project);assets.refresh();RuntimeLifecycle runtime;SceneDocument scene;
    for(const char* id:{"hero","target"}){SceneNode node;node.id=id;scene.nodes.push_back(node);}runtime.loadScene(scene);runtime.start();
    auto hero=runtime.entity("hero"),target=runtime.entity("target");
    runtime.world().addComponent(hero,game::Script{"assets:/controller.lua",true});GameRuntime game(runtime,application::systems(),application::explorationConfiguration());ScriptRuntime scripts(runtime,game,assets);
    scripts.update(1.0/60);check(!scripts.errors().empty()&&scripts.errors().back().find("transaction rollback")!=std::string::npos,"Failed init must execute bounded entity lookup");
    check(runtime.world().tryGet<ecs::TransformComponent>(target)->translation[0]==0,"Failed init must roll back other entity properties");
    write("local target; function init() target=self:find('target'); target:set('azure.transform','translation',{2,3,4}); assert(self:find('missing')==nil) end function interact(actor) self:set('azure.transform','translation',{8,0,0}) end function update(dt) target:get('azure.transform','translation') end");
    scripts.reloadChanged();scripts.update(1.0/60);
    check(scripts.activeCount()==1&&runtime.world().tryGet<ecs::TransformComponent>(target)->translation[0]==2,"Successful init must commit all queried entities");
    scripts.dispatchInteraction({target,hero,"target","hero","",runtime.sceneRevision()});
    check(runtime.world().tryGet<ecs::TransformComponent>(hero)->translation[0]==8,"Interaction callback must use normal script guards");
    runtime.world().destroyEntity(target);SceneNode replacement;replacement.id="replacement";runtime.deferSpawn(replacement);runtime.beginFrame(1.0/60);scripts.update(1.0/60);
    check(scripts.activeCount()==0&&scripts.errors().back().find("handle is stale")!=std::string::npos,"Saved query handles must reject numeric entity reuse");
    write("function init() self:remove_component('azure.rigid-body'); self:spawn('marker','',{1,2,3}); error('abort structural effects') end");
    runtime.world().addComponent(hero,game::RigidBody{});scripts.reloadChanged();runtime.beginFrame(1.0/60);
    check(runtime.world().has<game::RigidBody>(hero)&&runtime.entity("marker")==ecs::kInvalidEntity,"Failed init must isolate structural effects");
    write("function init() self:remove_component('azure.rigid-body'); self:spawn('marker','',{1,2,3}) end");scripts.reloadChanged();runtime.beginFrame(1.0/60);
    check(!runtime.world().has<game::RigidBody>(hero)&&runtime.entity("marker")!=ecs::kInvalidEntity,"Successful structural effects must commit at a frame boundary");
    std::cout<<"Lua entity queries, cross-entity init transaction, stale handles, interaction and structural effects passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';std::filesystem::remove_all(root);return 1;}
std::filesystem::remove_all(root);}
