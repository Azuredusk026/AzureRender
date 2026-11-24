#include "runtime/GameRuntime.hpp"
#include "app/ProjectRuntimeAssembly.hpp"
#include "runtime/LevelSession.hpp"
#include "runtime/GameInputReplay.hpp"
#include "runtime/ScriptRuntime.hpp"
#include <iostream>
#include <stdexcept>
using namespace azurerender;
void check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
template<class F> void rejects(F&& function) { bool failed=false;try { function(); } catch(const std::exception&) { failed=true; } check(failed,"Invalid input must be rejected"); }
struct CountingSystem final:IRuntimeSystem {
    std::vector<std::string>& log;std::string id;bool fail;
    CountingSystem(std::vector<std::string>& trace,std::string name,bool failure):log(trace),id(std::move(name)),fail(failure) {}
    void initialize(RuntimeSystemContext&) override { log.push_back("start:"+id);if(fail)throw std::runtime_error("Initialization failure"); }
    void sceneChanged(RuntimeSystemContext&) override { log.push_back("scene:"+id); }
    void fixedStep(RuntimeSystemContext& c) override { check(c.fixedStep==1.0/60,"Fixed step must be 60 Hz");log.push_back("step:"+id); }
    void shutdown(RuntimeSystemContext&) noexcept override { log.push_back("stop:"+id); }
};
void composition() {
    using Json=nlohmann::json;
    RuntimeLifecycle runtime;runtime.start();std::vector<std::string> trace;
    SystemRegistry registry;
    registry.add("counter",[&](const Json& config) { return std::make_unique<CountingSystem>(trace,config.at("label").get<std::string>(),false); });
    registry.add("failure",[&](const Json&) { return std::make_unique<CountingSystem>(trace,"failure",true); });
    rejects([&] { registry.add("counter",[](const Json&) { return std::unique_ptr<IRuntimeSystem>{}; }); });
    rejects([&] { registry.create("missing",Json::object()); });
    registry.add("null",[](const Json&) { return std::unique_ptr<IRuntimeSystem>{}; });
    rejects([&] { registry.create("null",Json::object()); });
    Json config={{"schemaVersion",1},{"input",Json::object()},{"systems",Json::array({{{"id","counter"},{"config",{{"label","one"}}}}})}};
    { GameRuntime game(runtime,registry,config);game.advance(1.0/30);runtime.pause();game.advance(.1);runtime.step();game.advance(.1);runtime.resume();
      check(game.steps()==3,"Registered system must obey pause and single-step");
      SceneDocument replacement;runtime.replaceScene(replacement);game.advance(1.0/60);
      check(game.steps()==4,"Registered system must survive scene replacement"); }
    check(trace==std::vector<std::string>{"start:one","scene:one","step:one","step:one","step:one","scene:one","step:one","stop:one"},"Lifecycle and scene callbacks must execute in declared order");
    trace.clear();config["systems"].push_back({{"id","failure"}});
    rejects([&] { GameRuntime game(runtime,registry,config); });
    check(trace==std::vector<std::string>{"start:one","start:failure","stop:failure","stop:one"},"Initialization failure must close in reverse order");
    for(const auto& invalid:std::vector<Json>{Json{{"schemaVersion",2}},Json{{"schemaVersion",1},{"input",Json::object()},{"systems",Json::object()}},Json{{"schemaVersion",1},{"input",Json::object()},{"systems",{{{"id","missing"}}}}}})
        rejects([&] { GameRuntime game(runtime,registry,invalid); });
    GameRuntime bare(runtime);bare.input().key(87,true);bare.advance(1.0/60);
    check(!bare.input().down("move-forward")&&!bare.hasCamera(),"Core scheduling must not install application defaults");
    trace.clear();config["systems"].erase(1);
    { GameRuntime game(runtime,registry,config);game.advance(1.0/60);
      runtime.defer([&](auto&) { SceneDocument next;runtime.replaceScene(next); });game.advance(1.0/60);
      check(trace==std::vector<std::string>{"start:one","scene:one","step:one","scene:one"},"Deferred scene change must notify systems and preserve frame cancellation");
      game.advance(1.0/60);
      check(trace==std::vector<std::string>{"start:one","scene:one","step:one","scene:one","step:one"},"The following frame must step the notified new world"); }
    auto invalidVersion=config;invalidVersion["schemaVersion"]=1.5;
    rejects([&] { GameRuntime game(runtime,registry,invalidVersion); });
}
void configuration() {
    using Json=nlohmann::json;
    InputActions input;input.configure({{"navigate",{262,263}},{"sprint",{340,344}}});
    input.key(262,true);check(input.down("navigate"),"Configured arrow input must work");
    rejects([&] { input.configure({{"bad",{999}}}); });check(input.down("navigate"),"Invalid rebinding must preserve existing state");
    input.configure({{"navigate",{265}}});check(!input.down("navigate")&&!input.bound(262),"Rebinding must release and replace held keys");
    input.key(265,true);input.setFocused(false);input.setFocused(true);check(!input.down("navigate"),"Focus loss must release rebound controls");
    auto config=application::explorationConfiguration();auto registry=application::systems();
    RuntimeLifecycle runtime;SceneDocument scene;SceneNode ground;ground.id="floor";ground.translation={0,-1,0};scene.nodes.push_back(ground);
    SceneNode hero;hero.id="pilot";hero.translation={0,1,0};scene.nodes.push_back(hero);runtime.loadScene(scene);runtime.start();
    game::RigidBody floor;floor.halfExtent={50,1,50};runtime.world().addComponent(runtime.entity("floor"),floor);
    game::Character character;character.inputProfile="pilot";character.speed=2;runtime.world().addComponent(runtime.entity("pilot"),character);
    game::Animator animator;animator.locomotion=true;animator.locomotionProfile="motion";runtime.world().addComponent(runtime.entity("pilot"),animator);
    config["input"]={{"west",{263}},{"east",{262}},{"north",{265}},{"south",{264}},{"leap",{32}},{"boost",{340,344}},{"use",{70}}};
    config["systems"][0]["config"]["profiles"]={{"pilot",{{"left","west"},{"right","east"},{"forward","north"},{"back","south"},{"jump","leap"},{"sprint","boost"}}}};
    config["systems"][2]["config"]["profiles"]={{"motion",{{"idle","stationary"},{"moving","advance"},{"threshold",.1}}}};
    config["systems"][5]["config"]["action"]="use";
    GameRuntime game(runtime,registry,config);
    for(int i=0;i<60;++i)game.advance(1.0/60);
    auto* driven=runtime.world().tryGet<game::Animator>(runtime.entity("pilot"));
    check(driven->state=="stationary","Locomotion must use the configured idle state");
    auto replay=GameInputReplay::parse({{"schemaVersion",1},{"actions",Json::array({{{"frame",0},{"action","key"},{"key",265},{"down",true}}})}});
    replay.apply(0,game);game.input().key(344,true);
    for(int i=0;i<60;++i)game.advance(1.0/60);
    check(driven->state=="advance"&&std::abs(driven->playbackRate-2.5F)<.05F,"Custom actions and state names must drive sprint animation");
    auto bad=config;bad["systems"][0]["config"]["profiles"]["pilot"]["forward"]="unbound";
    bool unboundRejected=false;try { GameRuntime invalid(runtime,registry,bad); } catch(const std::exception&) { unboundRejected=true; }
    check(unboundRejected,"Unbound movement action must fail during assembly");
}
void dynamicInput() {
    using Json=nlohmann::json;
    RuntimeLifecycle runtime;runtime.start();auto registry=application::systems();
    GameRuntime game(runtime,registry,application::explorationConfiguration());
    auto replay=GameInputReplay::parse({{"schemaVersion",1},{"actions",Json::array({{{"frame",10},{"action","key"},{"key",87},{"down",false}}})}});
    replay.enqueue({{"action","focus"},{"focused",true}},0);
    replay.enqueue({{"action","key"},{"key",87},{"down",true}},1);
    replay.apply(1,game);
    check(game.input().down("move-forward")&&replay.consumed()==2,"Dynamic events must precede future script actions");
    replay.apply(10,game);
    check(!game.input().down("move-forward")&&replay.consumed()==3,"Script releases must survive queue insertion");
    rejects([&]{replay.enqueue({{"action","key"},{"key",999},{"down",true}},11);});
    rejects([&]{replay.enqueue({{"action","key"},{"key",87},{"down",1}},11);});
    for(unsigned i=0;i<9000;++i){replay.enqueue({{"action","focus"},{"focused",true}},11+i);replay.apply(11+i,game);}
    check(replay.consumed()==9003,"Queue compaction must preserve cumulative consumption");
    for(unsigned i=0;i<8192;++i)replay.enqueue({{"action","focus"},{"focused",true}},20000);
    rejects([&]{replay.enqueue({{"action","focus"},{"focused",true}},20000);});
    rejects([&]{GameInputReplay::parse({{"schemaVersion",1.0},{"actions",Json::array()}});});
}
void inspectorProject() {
    const auto project=Project::load(std::filesystem::path(AZURE_INSPECTOR_PROJECT)/"project.azureproject");
    check(project.runtimeConfiguration.at("systems").size()==2,"Inspector must use two independent shared systems");
    RuntimeLifecycle runtime;LevelSession levels(project,runtime);runtime.start();
    GameRuntime game(runtime,application::systems(),application::configuration(project));
    ScriptRuntime scripts(runtime,game,levels.assets());
    game.setBeforeStep([&](double delta) { scripts.update(delta); });
    unsigned selected=0;
    game.setInteractionHandler([&](const auto& event) { ++selected;scripts.dispatchInteraction(event); });
    game.input().key(70,true);game.advance(1.0/60);game.advance(1.0/60);
    check(game.hasCamera()&&selected==1,"Inspector must select an asset through generic interaction and an orbit camera");
    check(!runtime.world().has<game::Character>(runtime.entity("observer")),"Inspector observer must not depend on character movement");
    const auto* target=runtime.world().tryGet<game::Interactable>(runtime.entity("asset-a"));
    check(target&&!target->enabled&&target->prompt=="Selected by observer","Inspector Lua must consume the production interaction event");
    check(game.interactionTarget()&&game.interactionTarget()->node=="asset-b","Disabled asset must select the next generic target");
    check(scripts.errors().empty(),"Inspector scripts must remain valid");
    levels.request(project.startupScene);
    for(unsigned i=0;i<1000&&levels.loading();++i) { levels.poll();std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
    check(!levels.loading()&&levels.lastError().empty(),"Inspector level must reopen");game.advance(1.0/60);
    check(game.hasCamera()&&!game.input().down("select-asset")&&game.interactionTarget()->node=="asset-a","Inspector reopen must restore camera, inputs and assets");
}
int main() { try {
    RuntimeLifecycle runtime; SceneDocument scene;
    SceneNode actor; actor.id="actor"; scene.nodes.push_back(actor);
    SceneNode asset; asset.id="asset"; asset.translation={0,0,1}; scene.nodes.push_back(asset);
    runtime.loadScene(scene); runtime.start();
    const auto target=runtime.entity("asset");
    runtime.world().addComponent(target,game::Interactable{});
    game::Collectible annotation; annotation.collected=true;
    runtime.world().addComponent(target,annotation);
    PhysicsWorld physics; InteractionRuntime interactions;
    check(interactions.select(runtime,physics,runtime.entity("actor")).has_value(),
        "A generic asset target must remain selectable regardless of application task annotation");
    interactions.setPolicy([](const auto&,auto,auto) { return false; });
    check(!interactions.select(runtime,physics,runtime.entity("actor")),"Application policy must filter generic targets");
    interactions.setPolicy({});check(interactions.select(runtime,physics,runtime.entity("actor")).has_value(),"Missing policy must allow a valid target");
    composition();configuration();dynamicInput();inspectorProject();
    std::cout<<"Generic target selection passed\n";
} catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; } }
