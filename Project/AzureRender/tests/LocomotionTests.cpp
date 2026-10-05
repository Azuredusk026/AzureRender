#include "app/ProjectRuntimeAssembly.hpp"
#include "runtime/GameRuntime.hpp"
#include "runtime/GameInputReplay.hpp"
#include "reflection/Registry.hpp"
#include "runtime/GameplayKeys.hpp"
#include <cmath>
#include <functional>
#include <iostream>
#include <stdexcept>

using namespace azurerender;
namespace {
void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
SceneDocument fixtureScene() {
    SceneDocument result;
    SceneNode ground; ground.id="ground"; ground.translation={0,-1,0}; result.nodes.push_back(ground);
    SceneNode hero; hero.id="hero"; hero.translation={0,1,0}; result.nodes.push_back(hero);
    return result;
}
void configure(RuntimeLifecycle& runtime) {
    game::RigidBody floor; floor.halfExtent={50,1,50};
    runtime.world().addComponent(runtime.entity("ground"),floor);
    game::Character hero; hero.speed=2; hero.forwardYaw=180;
    runtime.world().addComponent(runtime.entity("hero"),hero);
}
void setup(RuntimeLifecycle& runtime) {
    runtime.loadScene(fixtureScene()); configure(runtime); runtime.start();
}
std::array<float,3> position(RuntimeLifecycle& runtime) {
    return runtime.world().tryGet<ecs::TransformComponent>(runtime.entity("hero"))->translation;
}
float speed(GameRuntime& game,RuntimeLifecycle& runtime) {
    const auto velocity=game.physics().velocity(runtime.entity("hero"));
    return std::hypot(velocity[0],velocity[2]);
}
void settle(GameRuntime& game) { for(int i=0;i<60;++i)game.advance(1.0/60); }
}
int main() {
    unsigned failures=0;
    const auto test=[&](const char* name,const std::function<void()>& body) {
        try { body(); std::cout<<"PASS "<<name<<'\n'; }
        catch(const std::exception& error) { ++failures;std::cerr<<"FAIL "<<name<<": "<<error.what()<<'\n'; }
    };
    test("both Shift keys and release",[] {
        InputActions input;input.configure(application::explorationConfiguration().at("input"));
        input.key(340,true);check(input.down("sprint"),"Left Shift must activate sprint");
        input.key(344,true);input.key(340,false);check(input.down("sprint"),"Right Shift must keep sprint held");
        input.key(344,false);check(!input.down("sprint"),"Both released must stop sprint");
        input.key(340,true);input.setFocused(false);input.setFocused(true);
        check(!input.down("sprint"),"Focus loss must release sprint");
        input.bind("sprint",344);input.key(340,true);check(!input.down("sprint"),"bind must replace all keys");
        input.bindAdditional("sprint",340);check(input.down("sprint"),"Additional binding must retain both keys");
        input.bindAdditional("sprint",340);input.key(340,false);input.key(344,true);
        check(input.down("sprint"),"Duplicate additional binding must preserve the other key");
    });
    test("Character migration and limits",[] {
        auto registry=reflection::makeRuntimeRegistry();game::Character value;
        const auto encoded=registry.encode("azure.character",&value);
        check(encoded.at("version")==4,"Character must serialize version four");
        check(encoded.at("data").at("sprintMultiplier")==2.5F,"Sprint default must be 2.5");
        for(int version:{1,2}) {
            auto candidate=encoded;candidate["data"]["sprintMultiplier"]=3.5;
            registry.decode("azure.character",&value,candidate);
            registry.decode("azure.character",&value,{{"type","azure.character"},{"version",version},{"data",{{"speed",3}}}});
            check(registry.encode("azure.character",&value).at("data").at("sprintMultiplier")==2.5F,"Migration must add the default");
        }
        const auto before=registry.encode("azure.character",&value);
        for(float invalid:{.99F,4.01F}) {
            auto candidate=before;candidate["data"]["sprintMultiplier"]=invalid;candidate["data"]["speed"]=9;
            bool rejected=false;try {registry.decode("azure.character",&value,candidate);}catch(const std::exception&){rejected=true;}
            check(rejected&&registry.encode("azure.character",&value)==before,"Invalid sprint multiplier must be rejected atomically");
        }
    });
    test("walk sprint diagonal and idle",[] {
        RuntimeLifecycle runtime;setup(runtime);GameRuntime game(runtime,application::systems(),application::explorationConfiguration());settle(game);
        game.input().key(340,true);settle(game);check(speed(game,runtime)<.001F,"Shift alone must stay still");
        game.input().key(87,true);settle(game);check(std::abs(speed(game,runtime)-5)<.1F,"Sprint speed must be five meters per second");
        game.input().key(68,true);settle(game);check(std::abs(speed(game,runtime)-5)<.1F,"Diagonal sprint must normalize speed");
        game.input().key(340,false);settle(game);check(std::abs(speed(game,runtime)-2)<.04F,"Release Shift must return to walking");
        game.input().setFocused(false);settle(game);check(speed(game,runtime)<.001F,"Focus loss must stop movement");
    });
    test("pause releases held controls",[] {
        RuntimeLifecycle runtime;setup(runtime);GameRuntime game(runtime,application::systems(),application::explorationConfiguration());settle(game);
        game.input().key(87,true);game.input().key(340,true);game.advance(1.0/60);
        runtime.pause();game.advance(1.0/60);
        check(!game.input().down("move-forward")&&!game.input().down("sprint"),"Pause must release held movement and sprint");
        runtime.resume();settle(game);check(speed(game,runtime)<.001F,"Resume must brake to rest");
    });
    test("restart preserves focus and clears controls",[] {
        RuntimeLifecycle runtime;setup(runtime);GameRuntime game(runtime,application::systems(),application::explorationConfiguration());settle(game);
        game.input().key(87,true);game.input().key(344,true);
        runtime.replaceScene(fixtureScene(),configure);game.advance(1.0/60);
        check(!game.input().down("move-forward")&&!game.input().down("sprint"),"Restart must clear held controls");
        game.input().setFocused(false);runtime.replaceScene(fixtureScene(),configure);game.advance(1.0/60);
        check(!game.input().focused(),"Restart must preserve lost focus");
    });
    test("30 60 144 Hz fixed step",[] {
        std::array<float,3> baseline{};unsigned baselineSteps=0;
        for(int hz:{30,60,144}) {
            RuntimeLifecycle runtime;setup(runtime);GameRuntime game(runtime,application::systems(),application::explorationConfiguration());settle(game);
            game.input().key(87,true);game.input().key(344,true);
            for(int i=0;i<hz*2;++i)game.advance(1.0/hz);
            check(std::abs(speed(game,runtime)-5)<.1F,"All frame rates must reach sprint speed");
            if(hz==30){baseline=position(runtime);baselineSteps=static_cast<unsigned>(game.steps());}
            else {for(unsigned axis=0;axis<3;++axis)check(std::abs(position(runtime)[axis]-baseline[axis])<1e-5F,"Fixed-step positions must agree");
                check(game.steps()==baselineSteps,"Fixed-step counts must agree");}
        }
    });
    test("replay accepts both Shift keys",[] {
        check(isGameplayKey(340)&&isGameplayKey(344)&&!isGameplayKey(999),"Physical keys must use the shared gameplay domain");
        RuntimeLifecycle runtime;setup(runtime);GameRuntime game(runtime,application::systems(),application::explorationConfiguration());
        auto replay=GameInputReplay::parse({{"schemaVersion",1},{"actions",{
            {{"frame",0},{"action","key"},{"key",340},{"down",true}},
            {{"frame",1},{"action","key"},{"key",344},{"down",true}}}}});
        replay.apply(1,game);check(game.input().down("sprint")&&replay.consumed()==2,"Replay must share physical input actions");
    });
    return failures?1:0;
}
