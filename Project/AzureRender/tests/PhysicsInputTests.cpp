#include "runtime/GameRuntime.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace azurerender;
void check(bool value) { if (!value) throw std::runtime_error("Physics/input contract failed"); }
void setup(RuntimeLifecycle& runtime) {
 SceneDocument scene;SceneNode ground;ground.id="ground";ground.translation={0,-1,0};scene.nodes.push_back(ground);
 SceneNode hero;hero.id="hero";hero.translation={0,3,0};scene.nodes.push_back(hero);
 SceneNode trigger;trigger.id="trigger";trigger.translation={2,1,0};scene.nodes.push_back(trigger);
 runtime.loadScene(scene);game::RigidBody floor;floor.halfExtent={10,1,10};runtime.world().addComponent(runtime.entity("ground"),floor);
 runtime.world().addComponent(runtime.entity("hero"),game::Character{});
 game::RigidBody sensor;sensor.halfExtent={0.7F,2,2};sensor.trigger=true;runtime.world().addComponent(runtime.entity("trigger"),sensor);
 runtime.start();
}
int main() {try {
 InputActions input;input.bind("jump",32);input.key(32,true);input.key(32,true);check(input.pressed("jump") && input.down("jump"));
 input.endStep();check(!input.pressed("jump") && input.down("jump"));input.setFocused(false);check(!input.down("jump"));
 input.setFocused(true);input.key(82,true);check(input.pressed("restart"));input.endStep();check(!input.pressed("restart"));
 RuntimeLifecycle runtime;setup(runtime);GameRuntime game(runtime);for(int i=0;i<180;++i)game.advance(1.0/60.0);
 auto hero=runtime.entity("hero");auto* transform=runtime.world().tryGet<ecs::TransformComponent>(hero);
 check(transform->translation[1]>0.8F && transform->translation[1]<1.2F);check(game.physics().grounded(hero));
 auto hit=game.physics().raycast({0,5,0},{0,-10,0});check(hit.has_value() && hit->entity!=ecs::kInvalidEntity);
 bool entered=false,exited=false;game.setEventHandler([&](const PhysicsEvent& event){entered|=event.entered;exited|=!event.entered;});
 game.input().key(68,true);for(int i=0;i<90;++i)game.advance(1.0/60.0);check(entered && exited && transform->translation[0]>5);
 game.input().key(68,false);auto position=transform->translation;runtime.pause();game.advance(0.1);check(transform->translation==position);
 game.input().key(32,true);runtime.step();game.advance(0.1);check(transform->translation[1]>position[1]);game.input().key(32,false);
 runtime.resume();runtime.defer([hero](auto& world){world.destroyEntity(hero);});game.advance(1.0/60.0);check(!game.physics().contains(hero));
 runtime.world().createEntity();
 SceneDocument replacement;SceneNode replacementNode;replacementNode.id="hero";replacementNode.translation={0,5,0};replacement.nodes.push_back(replacementNode);
 runtime.replaceScene(replacement,[&](auto& candidate){candidate.world().addComponent(candidate.entity("hero"),game::Character{});});
 game.advance(1.0/60.0);check(runtime.world().tryGet<ecs::TransformComponent>(runtime.entity("hero"))->translation[1]>4.9F);
 RuntimeLifecycle first,second;setup(first);setup(second);GameRuntime a(first),b(second);
 for(int i=0;i<60;++i)a.advance(1.0/60.0);for(int i=0;i<20;++i)b.advance(0.05);
 const auto ya=first.world().tryGet<ecs::TransformComponent>(first.entity("hero"))->translation[1];
 const auto yb=second.world().tryGet<ecs::TransformComponent>(second.entity("hero"))->translation[1];check(std::abs(ya-yb)<0.0001F && a.steps()==b.steps());
 std::cout<<"Jolt character, raycast, triggers, fixed-step, pause, input focus and deletion passed\n";
 }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
