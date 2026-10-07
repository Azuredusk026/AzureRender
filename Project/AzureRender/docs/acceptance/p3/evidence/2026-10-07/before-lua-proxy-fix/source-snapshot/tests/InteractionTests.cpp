#include "app/ProjectRuntimeAssembly.hpp"
#include "runtime/InteractionRuntime.hpp"
#include "runtime/GameRuntime.hpp"
#include "runtime/ComponentCodec.hpp"
#include <iostream>
#include <stdexcept>
using namespace azurerender;
void check(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
int main(){try{
    RuntimeLifecycle runtime;SceneDocument scene;
    for(const char* id:{"hero","beta","alpha","blocker"}){SceneNode node;node.id=id;scene.nodes.push_back(node);}
    runtime.loadScene(scene);runtime.start();
    const auto hero=runtime.entity("hero"),alpha=runtime.entity("alpha"),beta=runtime.entity("beta");
    runtime.world().tryGet<ecs::TransformComponent>(alpha)->translation={0,0,-1};
    runtime.world().tryGet<ecs::TransformComponent>(beta)->translation={0,0,1};
    runtime.world().addComponent(alpha,game::Interactable{});runtime.world().addComponent(beta,game::Interactable{});
    PhysicsWorld physics;InteractionRuntime interactions;
    check(interactions.select(runtime,physics,hero)->node=="alpha","Equal distances must use stable node identity");
    runtime.world().tryGet<game::Interactable>(alpha)->enabled=false;
    check(interactions.select(runtime,physics,hero)->node=="beta","Unavailable targets must be excluded");
    runtime.world().tryGet<game::Interactable>(alpha)->enabled=true;
    const auto blocker=runtime.entity("blocker");runtime.world().tryGet<ecs::TransformComponent>(blocker)->translation={0,1,-.5F};
    game::RigidBody wall;wall.halfExtent={.5F,.5F,.1F};runtime.world().addComponent(blocker,wall);physics.step(runtime,1.0F/60,{});
    check(interactions.select(runtime,physics,hero)->node=="beta","Walls must block target visibility");
    runtime.world().removeComponent<game::RigidBody>(blocker);physics.step(runtime,1.0F/60,{});
    check(interactions.select(runtime,physics,hero)->node=="alpha","Target must recover when visibility is restored");
    runtime.world().tryGet<ecs::TransformComponent>(hero)->translation={100,0,0};
    check(!interactions.select(runtime,physics,hero),"Leaving range must clear the target");
    runtime.world().tryGet<ecs::TransformComponent>(hero)->translation={0,0,0};runtime.world().destroyEntity(alpha);
    const auto reused=runtime.world().createEntity();runtime.world().addComponent(reused,ecs::TransformComponent{});
    runtime.world().addComponent(reused,game::Interactable{});
    check(interactions.select(runtime,physics,hero)->node=="beta","Reused numeric entities must not inherit stale target identity");
    game::Character character;character.speed=0;runtime.world().addComponent(hero,character);GameRuntime game(runtime,application::systems(),application::explorationConfiguration());
    unsigned callbacks=0;game.setInteractionHandler([&](const auto& event){check(event.node=="beta","Interaction event must carry the selected target");++callbacks;});
    game.input().key(69,true);game.advance(1.0/30);game.advance(1.0/60);
    check(callbacks==1,"Held interaction must dispatch one press edge");
    runtime.world().destroyEntity(beta);game.input().key(69,false);game.input().key(69,true);game.advance(1.0/60);
    check(callbacks==1&&!game.interactionTarget(),"Deleted targets must clear before a new interaction");
    SceneNode spawned;spawned.id="spawned";spawned.resourceId="";
    runtime.deferSpawn(spawned,[](auto& world,auto entity){world.addComponent(entity,game::Collectible{});});
    check(runtime.entity("spawned")==ecs::kInvalidEntity,"Spawn must wait for a frame boundary");
    runtime.beginFrame(1.0/60);check(runtime.world().has<game::Collectible>(runtime.entity("spawned")),"Spawn must install components and stable identity");
    check(runtime.snapshotScene().nodes.back().id=="spawned","Spawn must participate in scene snapshots");
    SceneNode pending;pending.id="pending";runtime.deferSpawn(pending);bool rejected=false;
    try{runtime.deferSpawn(pending);}catch(const std::invalid_argument&){rejected=true;}
    check(rejected,"Duplicate pending spawns must be rejected before the transaction commits");
    SceneNode missingResource;missingResource.id="missing-mesh";missingResource.resourceId="absent";rejected=false;
    try{runtime.deferSpawn(missingResource);}catch(const std::invalid_argument&){rejected=true;}
    check(rejected,"Invalid spawn resources must fail inside the requesting callback");
    runtime.beginFrame(1.0/60);
    const auto beforeFailure=runtime.world().entityCount();
    SceneNode failed;failed.id="failed-configure";
    runtime.deferSpawn(failed,[](auto& world,auto entity){world.addComponent(entity,game::TaskState{});throw std::runtime_error("Configure failed");});
    SceneNode following;following.id="following-configure";runtime.deferSpawn(following);
    bool failedConfigure=false;try{runtime.beginFrame(1.0/60);}catch(const std::runtime_error&){failedConfigure=true;}
    check(failedConfigure&&runtime.world().entityCount()==beforeFailure&&runtime.entity(failed.id)==ecs::kInvalidEntity,"Failed spawn configuration must release the candidate entity");
    runtime.beginFrame(1.0/60);
    check(runtime.entity(following.id)!=ecs::kInvalidEntity,"A failed deferred operation must preserve the remaining queue for recovery");
    RuntimeLifecycle replacementRuntime;replacementRuntime.loadScene(scene);replacementRuntime.start();
    replacementRuntime.defer([&](auto&){SceneDocument next;SceneNode node;node.id="next-level";next.nodes.push_back(node);replacementRuntime.replaceScene(next);});
    SceneNode oldSpawn;oldSpawn.id="old-level-spawn";replacementRuntime.deferSpawn(oldSpawn);
    replacementRuntime.beginFrame(1.0/60);
    check(replacementRuntime.entity(oldSpawn.id)==ecs::kInvalidEntity,"Replacing a scene during a transaction must cancel the old batch");
    auto registry=reflection::makeRuntimeRegistry();game::TaskState task;task.collected=3;
    auto envelope=registry.encode("azure.task-state",&task);game::TaskState restored;registry.decode("azure.task-state",&restored,envelope);
    check(restored.collected==3,"Task components must roundtrip through the production registry");
    std::cout<<"Interaction range, visibility, stable identity, press edges, spawn and task storage passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
