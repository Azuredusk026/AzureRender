#include "runtime/ScriptRuntime.hpp"
#include "runtime/LevelSession.hpp"
#include <filesystem>
#include <iostream>
#include <map>
#include <stdexcept>
using namespace azurerender;
void check(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
int main(){const auto root=std::filesystem::temp_directory_path()/"azure_playable_quest";
try{
    std::filesystem::remove_all(root);
    std::filesystem::copy(AZURE_QUEST_PROJECT,root,std::filesystem::copy_options::recursive);
    RuntimeLifecycle runtime;LevelSession levels(Project::load(root/"project.azureproject"),runtime);runtime.start();
    GameRuntime game(runtime);ScriptRuntime scripts(runtime,game,levels.assets());std::map<std::string,std::string> ui;
    scripts.setUiHandler([&](auto id,auto text){ui[id]=text;});
    scripts.setLevelHandler([&](auto reference){levels.request(reference);});
    game.setBeforeStep([&](double dt){scripts.update(dt);});
    game.setEventHandler([&](const auto& event){scripts.dispatch(event);});
    game.setInteractionHandler([&](const auto& event){scripts.dispatchInteraction(event);});
    game.advance(1.0/60);
    const auto hero=runtime.entity("hero:body");
    auto task=[&]{return *runtime.world().tryGet<game::TaskState>(runtime.entity("hero:body"));};
    auto interact=[&](const std::string& node){scripts.dispatchInteraction({hero,runtime.entity(node),"hero:body",node,"",runtime.sceneRevision()});};
    interact("artifact-0:body");check(task().collected==0,"Collecting before accepting the task must be rejected");
    interact("door:body");check(!task().doorOpened,"Locked door must retain collision before requirements are met");
    interact("guide:body");check(task().started,"Guide interaction must start the quest");
    for(const char* id:{"artifact-0:body","artifact-1:body","artifact-2:body"}){interact(id);interact(id);game.advance(1.0/60);}
    check(task().collected==3,"Repeated collection must count each artifact once");
    check(runtime.entity("artifact-0:body")==ecs::kInvalidEntity,"Collected objects must be destroyed at the next boundary");
    scripts.dispatch({runtime.entity("goal"),hero,true});check(!task().completed,"Goal must reject a locked gate");
    interact("door:body");interact("door:body");game.advance(1.0/60);
    check(task().doorOpened&&!game.physics().contains(runtime.entity("door:body")),"Opening the gate must release the physical body");
    runtime.world().tryGet<ecs::TransformComponent>(hero)->translation={0,0,-300};scripts.update(1.0/60);
    check(ui.at("route").find("gate")!=std::string::npos,"Route hints must follow the world Z coordinate in Lua vector3 values");
    scripts.dispatch({runtime.entity("checkpoint:body"),hero,true});
    scripts.dispatch({runtime.entity("goal"),hero,true});game.advance(1.0/60);
    check(task().completed&&ui.at("objective").find("complete")!=std::string::npos,"Goal must expose visible completion feedback");
    check(scripts.errors().empty()&&scripts.activeCount()==5,"Destroyed objects must release script entries");
    const auto stale=InteractionTarget{hero,runtime.entity("guide:body"),"hero:body","guide:body","",runtime.sceneRevision()};
    game.input().key(82,true);game.advance(1.0/60);check(levels.poll(),"Restart input must request a frame boundary level transaction");
    scripts.dispatchInteraction(stale);game.advance(1.0/60);
    check(!task().started&&task().collected==0&&!task().completed&&!task().doorOpened,"Restart must restore every task field and reject stale events");
    check(runtime.entity("artifact-0:body")!=ecs::kInvalidEntity&&game.physics().contains(runtime.entity("door:body")),"Restart must recreate artifacts and door collision");
    check(!runtime.world().tryGet<game::Checkpoint>(runtime.entity("checkpoint:body"))->activated,"Restart must reset checkpoint state");
    check(scripts.errors().empty()&&scripts.activeCount()==8,"Restart must restore all script entries");
    std::cout<<"Production quest, duplicate input, goal prerequisites, UI, destruction, stale events and restart passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';std::filesystem::remove_all(root);return 1;}
std::filesystem::remove_all(root);}
