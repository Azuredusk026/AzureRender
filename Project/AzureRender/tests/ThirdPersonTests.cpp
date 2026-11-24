#include "app/ProjectRuntimeAssembly.hpp"
#include "runtime/ThirdPersonCamera.hpp"
#include "runtime/GameRuntime.hpp"
#include "runtime/GameInputReplay.hpp"
#include "reflection/Registry.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace azurerender;
void check(bool value, const char* reason) { if (!value) throw std::runtime_error(reason); }
void setup(RuntimeLifecycle& runtime) {
    SceneDocument scene;
    SceneNode ground; ground.id="ground"; ground.translation={0,-1,0}; scene.nodes.push_back(ground);
    SceneNode hero; hero.id="hero"; hero.translation={0,1,0}; scene.nodes.push_back(hero);
    runtime.loadScene(scene);
    game::RigidBody floor; floor.halfExtent={30,1,30}; runtime.world().addComponent(runtime.entity("ground"),floor);
    runtime.world().addComponent(runtime.entity("hero"),game::Character{}); runtime.start();
}
int main() { try {
    ThirdPersonCamera camera; game::ThirdPersonCamera settings;
    camera.orbit(90, 200, settings);
    check(camera.pitch()<=settings.maximumPitch && camera.pitch()>=settings.minimumPitch,"Camera pitch must clamp");
    camera.zoom(100,settings); check(camera.distance()>=settings.minimumDistance,"Zoom must clamp");
    camera.reset({0,1,0},settings); camera.update({0,1,0},settings,1.0F/60,[](auto,auto,float){return .3F;});
    const auto compressed=camera.actualDistance();
    check(compressed < settings.distance*.4F,"Sphere obstruction must shrink camera");
    camera.update({0,1,0},settings,1.0F/60,[](auto,auto,float){return 1.0F;});
    check(camera.actualDistance()>compressed && camera.actualDistance()<settings.distance,"Released camera must recover smoothly");
    RuntimeLifecycle first,second,third;setup(first);setup(second);setup(third);
    GameRuntime a(first,application::systems(),application::explorationConfiguration()),b(second,application::systems(),application::explorationConfiguration()),c(third,application::systems(),application::explorationConfiguration());
    for(auto* game:{&a,&b,&c}){game->setCameraYaw(90);game->input().key(87,true);}
    for(int i=0;i<60;++i)a.advance(1.0/30);
    for(int i=0;i<120;++i)b.advance(1.0/60);
    for(int i=0;i<240;++i)c.advance(1.0/120);
    auto position=[](RuntimeLifecycle& r){return r.world().tryGet<ecs::TransformComponent>(r.entity("hero"))->translation;};
    const auto pa=position(first),pb=position(second),pc=position(third);
    check(pa[0]>6 && std::abs(pa[2])<.01F,"Movement must follow camera yaw");
    for(unsigned axis=0;axis<3;++axis)check(std::abs(pa[axis]-pb[axis])<1e-5F&&std::abs(pa[axis]-pc[axis])<1e-5F,"30/60/120 Hz simulation must agree");
    c.advance(1.0/120);const auto rendered=c.renderScene();
    const auto hero=std::find_if(rendered.nodes.begin(),rendered.nodes.end(),[](const auto& n){return n.id=="hero";});
    check(hero!=rendered.nodes.end()&&hero->translation[0]<position(third)[0],"Render pose must interpolate between fixed steps");
    RuntimeLifecycle angles;setup(angles);GameRuntime angleGame(angles,application::systems(),application::explorationConfiguration());angleGame.advance(1.0/60);
    angles.world().tryGet<ecs::TransformComponent>(angles.entity("hero"))->rotation[1]=359;
    angleGame.advance(1.0/120);
    check(std::abs(angleGame.renderScene().nodes.at(1).rotation[1]+.5F)<1e-5F,"Render rotation must interpolate across angle wrap by the shortest arc");
    third.pause();const auto paused=c.renderScene();
    check(paused.nodes.at(1).translation==position(third),"Pause must show the current simulation pose");
    const auto steps=c.steps();c.advance(.1);check(c.steps()==steps,"Pause freezes simulation");
    third.step();c.advance(.1);check(c.steps()==steps+1,"Single step advances exactly once");
    b.input().setFocused(false);for(int i=0;i<60;++i)b.advance(1.0/60);
    check(!b.input().down("move-forward"),"Focus loss must clear input");
    const auto old=position(second);b.advance(1.0/60);check(std::abs(position(second)[0]-old[0])<.001F,"Braking must stop movement");
    auto hit=b.physics().sphereSweep({0,2,0},{0,-4,0},.25F,second.entity("hero"));
    check(hit&&hit->entity==second.entity("ground")&&hit->fraction<.5F,"Sphere sweep must hit the floor and ignore character");
    auto registry=reflection::makeRuntimeRegistry();game::Character migrated;
    registry.decode("azure.character",&migrated,nlohmann::json::parse(R"({"type":"azure.character","version":1,"data":{"speed":3}})"));
    check(migrated.speed==3&&migrated.acceleration>0,"Version one components must gain controller defaults");
    game::ThirdPersonCamera invalidCamera;
    auto cameraEnvelope=registry.encode("azure.third-person-camera",&invalidCamera);
    cameraEnvelope["data"]["minimumDistance"]=6;
    bool rejected=false;
    try { registry.decode("azure.third-person-camera",&invalidCamera,cameraEnvelope); }
    catch(const std::invalid_argument&) { rejected=true; }
    check(rejected&&invalidCamera.minimumDistance==1,"Camera component must reject incompatible ranges atomically");
    RuntimeLifecycle cameraRuntime;setup(cameraRuntime);
    cameraRuntime.world().addComponent(cameraRuntime.entity("ground"),game::ThirdPersonCamera{});
    GameRuntime following(cameraRuntime,application::systems(),application::explorationConfiguration());following.advance(1.0/60);
    check(following.hasCamera(),"Configured camera must resolve its target");
    following.cameraInput(200,50,2);
    cameraRuntime.world().destroyEntity(cameraRuntime.entity("hero"));following.advance(1.0/60);
    check(!following.hasCamera(),"Deleted camera target must invalidate following");
    SceneDocument replacement;SceneNode newHero;newHero.id="hero";newHero.translation={10,1,0};replacement.nodes.push_back(newHero);
    cameraRuntime.replaceScene(replacement,[](auto& runtime){runtime.world().addComponent(runtime.entity("hero"),game::ThirdPersonCamera{});});
    following.advance(1.0/60);
    check(following.hasCamera()&&following.camera().yaw()==0&&following.renderCameraTarget()[0]==10,
          "Scene replacement must resolve the new target and reset camera interpolation");
    RuntimeLifecycle route;setup(route);GameRuntime controller(route,application::systems(),application::explorationConfiguration());
    for(int i=0;i<60;++i)controller.advance(1.0/60);
    auto addBox=[&](const char* id,std::array<float,3> p,std::array<float,3> extent,std::array<float,3> angles=std::array<float,3>{}){
        const auto entity=route.world().createEntity();route.world().addComponent(entity,ecs::TransformComponent{p,angles,{1,1,1}});
        game::RigidBody body;body.halfExtent=extent;route.world().addComponent(entity,body);(void)id;return entity;
    };
    addBox("step",{1.2F,.1F,0},{.4F,.1F,1});
    addBox("wall",{4,1,0},{.1F,1,2});
    addBox("corner",{3,1,1.1F},{1,1,.1F});
    controller.input().key(68,true);float peak=0;
    for(int i=0;i<80;++i){controller.advance(1.0/60);peak=std::max(peak,position(route)[1]);}
    check(peak>1.05F,"Configured stairs must climb a twenty-centimeter step");
    check(position(route)[0]<3.65F,"Character capsule must stop at a wall");
    controller.input().key(83,true);for(int i=0;i<80;++i)controller.advance(1.0/60);
    check(position(route)[2]<.75F,"Diagonal movement must preserve wall-corner collision");
    RuntimeLifecycle slope;setup(slope);GameRuntime slopeGame(slope,application::systems(),application::explorationConfiguration());
    slope.world().tryGet<ecs::TransformComponent>(slope.entity("hero"))->translation={0,1,2};
    auto ramp=slope.world().createEntity();slope.world().addComponent(ramp,ecs::TransformComponent{{0,.5F,0},{20,0,0},{1,1,1}});
    game::RigidBody rampBody;rampBody.halfExtent={1,.1F,1.5F};slope.world().addComponent(ramp,rampBody);
    for(int i=0;i<60;++i)slopeGame.advance(1.0/60);slopeGame.input().key(87,true);float slopePeak=0;
    for(int i=0;i<75;++i){slopeGame.advance(1.0/60);slopePeak=std::max(slopePeak,position(slope)[1]);}
    check(slopePeak>1.6F&&position(slope)[2]<-1,"Walkable slope must preserve grounded climbing");
    RuntimeLifecycle steep;setup(steep);
    steep.world().tryGet<ecs::TransformComponent>(steep.entity("hero"))->translation={0,1,2};
    steep.world().tryGet<game::Character>(steep.entity("hero"))->maximumSlope=15;
    auto steepRamp=steep.world().createEntity();steep.world().addComponent(steepRamp,ecs::TransformComponent{{0,.5F,0},{20,0,0},{1,1,1}});
    steep.world().addComponent(steepRamp,rampBody);GameRuntime steepGame(steep,application::systems(),application::explorationConfiguration());
    for(int i=0;i<60;++i)steepGame.advance(1.0/60);steepGame.input().key(87,true);
    for(int i=0;i<75;++i)steepGame.advance(1.0/60);
    check(position(steep)[2]>0,"Slope limit must block climbing the same ramp when configured below its angle");
    RuntimeLifecycle buffered;setup(buffered);buffered.world().tryGet<ecs::TransformComponent>(buffered.entity("hero"))->translation[1]=.935F;
    GameRuntime bufferGame(buffered,application::systems(),application::explorationConfiguration());bufferGame.input().key(32,true);float jumpPeak=0;
    for(int i=0;i<35;++i){bufferGame.advance(1.0/60);jumpPeak=std::max(jumpPeak,position(buffered)[1]);}
    check(jumpPeak>1.8F,"Jump buffered before landing must fire once on contact");
    for(int i=0;i<100;++i)bufferGame.advance(1.0/60);
    check(bufferGame.physics().grounded(buffered.entity("hero")),"Held jump must not retrigger after landing");
    RuntimeLifecycle ledge;setup(ledge);GameRuntime ledgeGame(ledge,application::systems(),application::explorationConfiguration());for(int i=0;i<60;++i)ledgeGame.advance(1.0/60);
    ledge.world().destroyEntity(ledge.entity("ground"));for(int i=0;i<3;++i)ledgeGame.advance(1.0/60);
    ledgeGame.input().key(32,true);ledgeGame.advance(1.0/60);
    check(ledgeGame.physics().velocity(ledge.entity("hero"))[1]>4,"Coyote jump must work within one hundred milliseconds");
    RuntimeLifecycle expired;setup(expired);GameRuntime expiredGame(expired,application::systems(),application::explorationConfiguration());for(int i=0;i<60;++i)expiredGame.advance(1.0/60);
    expired.world().destroyEntity(expired.entity("ground"));for(int i=0;i<12;++i)expiredGame.advance(1.0/60);
    expiredGame.input().key(32,true);expiredGame.advance(1.0/60);
    check(expiredGame.physics().velocity(expired.entity("hero"))[1]<0,"Coyote jump must expire outside its window");
    auto replay=GameInputReplay::parse(nlohmann::json::parse(R"({"schemaVersion":1,"actions":[{"frame":0,"action":"key","key":87,"down":true},{"frame":2,"action":"focus","focused":false},{"frame":3,"action":"focus","focused":true}]})"));
    replay.apply(0,a);check(a.input().down("move-forward"),"Replay must use the normal input actions");
    replay.apply(3,a);check(!a.input().down("move-forward")&&replay.consumed()==3,"Replay focus loss must release held keys");
    auto loadingReplay=GameInputReplay::parse(nlohmann::json::parse(R"({"schemaVersion":1,"actions":[{"frame":0,"action":"preload-level","reference":"assets:/next.azurelevel"},{"frame":1,"action":"cancel-level"}]})"));
    bool missingSession=false;try{loadingReplay.apply(0,a);}catch(const std::exception&){missingSession=true;}
    check(missingSession,"Level replay must reject a missing level session");
    std::cout<<"Third-person camera, obstruction, camera-relative motion, fixed-step, focus, sweep and migration passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;} }
