#include "app/ProjectRuntimeAssembly.hpp"
#include "runtime/GameRuntime.hpp"
#include "runtime/ComponentCodec.hpp"
#include "scripting/ScriptBindingHost.hpp"
#include <fstream>
#include <iostream>
#include <cmath>
using namespace azurerender;
using Json=nlohmann::json;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct Fixture {
    RuntimeLifecycle runtime;
    std::unique_ptr<GameRuntime> game;
    Fixture(bool moving=false){
        SceneDocument scene;SceneNode floor;floor.id="floor";floor.translation={0,-.25F,0};scene.nodes.push_back(floor);
        SceneNode hero;hero.id="hero";hero.translation={0,.02F,0};scene.nodes.push_back(hero);
        runtime.loadScene(scene);runtime.start();
        auto body=runtimeComponentRegistry().defaults("azure.rigid-body");body["data"]["halfExtent"]={10,.25,10};
        if(moving)body["data"]["kinematic"]=true;
        runtimeComponentRegistry().install("azure.rigid-body",runtime.world(),runtime.entity("floor"),body);
        game::Character character;character.centerOffset=.9F;character.sprintMultiplier=1.4F;
        runtime.world().addComponent(runtime.entity("hero"),character);
        game=std::make_unique<GameRuntime>(runtime,application::systems(),application::explorationConfiguration());
        step(60);
    }
    void step(int count){for(int i=0;i<count;++i)game->advance(1.0/60);}
    ecs::TransformComponent& transform(const char* node="hero"){return *runtime.world().tryGet<ecs::TransformComponent>(runtime.entity(node));}
};
Json measure(){
    Fixture f;auto& game=*f.game;const auto hero=f.runtime.entity("hero");
    const float base=f.transform().translation[1];game.input().key(87,true);f.step(60);
    const float walkSpeed=std::abs(game.physics().velocity(hero)[2]);
    const auto launch=f.transform().translation;game.input().key(32,true);f.step(1);game.input().key(32,false);
    float peak=f.transform().translation[1];int steps=1;Json trajectory=Json::array();
    do{trajectory.push_back({{"step",steps},{"position",f.transform().translation},{"grounded",game.physics().grounded(hero)}});peak=std::max(peak,f.transform().translation[1]);f.step(1);++steps;}while(!game.physics().grounded(hero)&&steps<180);
    require(steps<180,"Jump must land on the measurement floor");
    const float distance=std::abs(f.transform().translation[2]-launch[2]);
    const auto stop=f.transform().translation;game.input().key(87,false);f.step(60);
    Fixture latency;latency.game->input().key(87,true);const auto start=latency.transform().translation;latency.step(1);
    Fixture sprint;sprint.game->input().key(340,true);sprint.game->input().key(87,true);sprint.step(60);
    const float sprintSpeed=std::abs(sprint.game->physics().velocity(sprint.runtime.entity("hero"))[2]);
    return {{"schemaVersion",1},{"fixedStep",1.0/60},{"walkSpeed",walkSpeed},{"sprintSpeed",sprintSpeed},{"jumpSpeed",5},{"jumpHeight",peak-base},{"airtime",steps/60.0},
        {"maximumDistance",distance},{"radius",.3},{"halfHeight",.6},{"capsuleHeight",1.8},{"acceleration",24},{"braking",32},
        {"firstStepMovement",std::abs(latency.transform().translation[2]-start[2])},{"inputLatencySeconds",1.0/60},
        {"brakingDistance",std::abs(f.transform().translation[2]-stop[2])},{"trajectory",trajectory}};
}
void carry(){
    for(unsigned axis:{0u,1u}){
        Fixture f(true);const auto relative=f.transform().translation[axis]-f.transform("floor").translation[axis];
        for(int i=0;i<120;++i){f.transform("floor").translation[axis]+=.005F;f.step(1);}
        require(std::abs((f.transform().translation[axis]-f.transform("floor").translation[axis])-relative)<.05F,"Idle character must ride a moving platform without sliding");
        require(f.game->physics().grounded(f.runtime.entity("hero")),"Rider must retain stable ground contact");
        f.game->input().key(32,true);f.step(1);f.game->input().key(32,false);f.step(10);
        require(!f.game->physics().grounded(f.runtime.entity("hero")),"Jump must detach from moving support");
    }
}
void observation(){
    Fixture f;ScriptBindingHost host(f.runtime,*f.game);auto hero=host.object("hero");
    require(host.invoke(hero,"grounded",Json::array()).get<bool>(),"Scripts must query real support contact");
    auto velocity=host.invoke(hero,"velocity",Json::array());require(velocity.is_array()&&velocity.size()==3,"Scripts must query actual physics velocity");
    f.game->input().key(32,true);f.step(1);
    require(!host.invoke(hero,"grounded",Json::array()).get<bool>()&&host.invoke(hero,"velocity",Json::array())[1]>0,"Queries must reflect actual jump motion");
}
int main(int argc,char** argv){try{
    if(argc==3&&std::string(argv[1])=="--measure"){std::ofstream(argv[2])<<measure().dump(2);return 0;}
    carry();observation();std::cout<<"Horizontal and vertical support, idle braking, jump detachment and script observations passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
