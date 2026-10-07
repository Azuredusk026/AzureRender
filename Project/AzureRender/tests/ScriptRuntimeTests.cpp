#include "app/ProjectRuntimeAssembly.hpp"
#include "runtime/ScriptRuntime.hpp"
#include "runtime/LevelSession.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
using namespace azurerender;
void check(bool value) { if(!value)throw std::runtime_error("Script runtime contract failed"); }
void write(const std::filesystem::path& p,const std::string& data){std::ofstream(p)<<data;}
int main(){auto root=std::filesystem::temp_directory_path()/"azure_script_tests";std::filesystem::remove_all(root);
 try {
  Project::create(root,"Scripts");auto project=Project::load(root/"project.azureproject");
  write(root/"assets/good.lua",R"(function update(dt) self:set('azure.transform','translation',{3,4,5}) end
function trigger(other, entered) if entered then self:load_level('assets:/startup.azscene') end end)");
  write(root/"assets/bad.lua","function update(dt) error('isolated error') end");
  write(root/"assets/loop.lua","function update(dt) while true do end end");
  AssetDatabase assets(project);assets.refresh();RuntimeLifecycle runtime;SceneDocument scene;
  for(const char* name:{"good","bad","loop"}) {SceneNode node;node.id=name;scene.nodes.push_back(node);}
  runtime.loadScene(scene);runtime.start();GameRuntime game(runtime,application::systems(),application::explorationConfiguration());
  for(const char* name:{"good","bad","loop"})runtime.world().addComponent(runtime.entity(name),game::Script{"assets:/"+std::string(name)+".lua",true});
  ScriptRuntime scripts(runtime,game,assets);std::string requested;scripts.setLevelHandler([&](std::string path){requested=path;});
  scripts.update(1.0/60.0);check(scripts.activeCount()==1 && scripts.errors().size()==2);
  auto good=runtime.entity("good");check(runtime.world().tryGet<ecs::TransformComponent>(good)->translation[0]==3);
  scripts.dispatch({good,runtime.entity("bad"),true});check(requested=="assets:/startup.azscene");
  write(root/"assets/good.lua","function update(");scripts.reloadChanged();scripts.update(1.0/60.0);
  check(scripts.activeCount()==1 && runtime.world().tryGet<ecs::TransformComponent>(good)->translation[1]==4);
  write(root/"assets/good.lua","function update(dt) self:set('azure.transform','translation',{7,8,9}) end");scripts.reloadChanged();scripts.update(1.0/60.0);
  check(runtime.world().tryGet<ecs::TransformComponent>(good)->translation[2]==9);
  write(root/"assets/good.lua",R"(function init() self:set('azure.transform','translation',{100,100,100});self:load_level('bad');error('init failed') end)");
  scripts.reloadChanged();scripts.update(1.0/60.0);
  check(scripts.activeCount()==1 && runtime.world().tryGet<ecs::TransformComponent>(good)->translation[0]==7 && requested=="assets:/startup.azscene");
  write(root/"assets/good.lua","self:set('azure.transform','translation',{100,100,100})");scripts.reloadChanged();scripts.update(1.0/60.0);
  check(runtime.world().tryGet<ecs::TransformComponent>(good)->translation[0]==7);
  write(root/"assets/good.lua",R"(function update(dt) self:destroy() end)");scripts.reloadChanged();scripts.update(1.0/60.0);runtime.beginFrame(1.0/60.0);scripts.update(1.0/60.0);
  check(!runtime.world().valid(good) && scripts.activeCount()==0);
  SceneDocument lifetime;SceneNode owner;owner.id="owner";lifetime.nodes.push_back(owner);
  runtime.replaceScene(lifetime);auto ownerEntity=runtime.entity("owner");
  runtime.world().addComponent(ownerEntity,game::Script{"assets:/good.lua",true});
  write(root/"assets/good.lua",R"(function update(dt) self:destroy() end
function shutdown() self:set('azure.transform','translation',{11,12,13}) end)");
  {ScriptRuntime scoped(runtime,game,assets);scoped.update(1.0/60.0);}
  check(runtime.world().tryGet<ecs::TransformComponent>(ownerEntity)->translation[0]==11);
  runtime.beginFrame(1.0/60.0);check(!runtime.world().valid(ownerEntity));
  // A queued handle must not delete a same-ID entity in a replacement World.
  runtime.replaceScene(lifetime);ownerEntity=runtime.entity("owner");
  runtime.world().addComponent(ownerEntity,game::Script{"assets:/good.lua",true});
  {ScriptRuntime scoped(runtime,game,assets);scoped.update(1.0/60.0);}
  runtime.replaceScene(lifetime);runtime.beginFrame(1.0/60.0);check(runtime.world().valid(runtime.entity("owner")));
  ownerEntity=runtime.entity("owner");runtime.world().addComponent(ownerEntity,game::Script{"assets:/good.lua",true});
  write(root/"assets/good.lua","function trigger(other, entered) self:get('azure.transform','translation') end");
  {ScriptRuntime stale(runtime,game,assets);stale.update(1.0/60.0);
   runtime.world().destroyEntity(ownerEntity);stale.dispatch({ownerEntity,ecs::kInvalidEntity,true});
   check(stale.activeCount()==0 && stale.errors().back().find("handle is stale")!=std::string::npos);}
  auto level=nlohmann::json::parse(R"({"schemaVersion":1,"id":"hot","resources":[],"nodes":[{"id":"hero","components":{"azure.script":{"type":"azure.script","version":1,"data":{"asset":"assets:/good.lua"}}}}]})");
  write(root/"assets/hot.azurelevel",level.dump());project.startupScene="assets:/hot.azurelevel";
  write(root/"assets/good.lua","function update(dt) self:set('azure.transform','translation',{1,2,3}) end");
  RuntimeLifecycle hot;hot.start();LevelSession session(project,hot);GameRuntime hotGame(hot,application::systems(),application::explorationConfiguration());
  ScriptRuntime hotScripts(hot,hotGame,session.assets());hotScripts.update(1.0/60.0);
  auto hero=hot.entity("hero");const auto revision=hot.sceneRevision();
  write(root/"assets/good.lua","function update(dt) self:set('azure.transform','translation',{4,5,6}) end");
  std::this_thread::sleep_for(std::chrono::milliseconds(550));
  check(!session.poll() && hot.sceneRevision()==revision && hot.entity("hero")==hero);
  hotScripts.reloadChanged();hotScripts.update(1.0/60.0);
  check(hot.world().tryGet<ecs::TransformComponent>(hero)->translation[2]==6);
  write(root/"assets/good.lua","function init() self.id='forged' end");
  hotScripts.reloadChanged();hotScripts.update(1.0/60.0);
  check(!hotScripts.errors().empty() && hotScripts.errors().back().find("read only")!=std::string::npos);
  check(hotScripts.activeCount()==1 && hot.world().tryGet<ecs::TransformComponent>(hero)->translation[2]==6);
  write(root/"assets/good.lua",R"(function init() self.counter=0 end
function update(dt) self.counter=self.counter+1;self:set('azure.transform','translation',{self.counter,0,0}) end)");
  hotScripts.reloadChanged();hotScripts.update(1.0/60.0);hotScripts.update(1.0/60.0);
  check(hotScripts.activeCount()==1 && hot.world().tryGet<ecs::TransformComponent>(hero)->translation[0]==2);
  write(root/"assets/good.lua",R"(function update(dt) self:set('azure.transform','translation',{7,0,0}) end
function shutdown() self:set('azure.transform','translation',{11,0,0});self:spawn('reserved','',{0,0,0}) end)");
  hotScripts.reloadChanged();hotScripts.update(1.0/60.0);
  write(root/"assets/good.lua",R"(function init() self:set('azure.transform','translation',{999,0,0});self:spawn('reserved','',{0,0,0}) end)");
  hotScripts.reloadChanged();
  check(hotScripts.activeCount()==1 && hot.world().tryGet<ecs::TransformComponent>(hero)->translation[0]==999);
  hot.beginFrame(0);check(hot.entity("reserved")!=ecs::kInvalidEntity);
  std::cout<<"Lua reflection, error and loop isolation, triggers, reload retention and entity cleanup passed\n";
 }catch(const std::exception& error){std::cerr<<error.what()<<'\n';std::filesystem::remove_all(root);return 1;}
 std::filesystem::remove_all(root);
}
