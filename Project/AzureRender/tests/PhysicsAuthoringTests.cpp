#include "runtime/PhysicsWorld.hpp"
#include "runtime/ComponentRegistry.hpp"
#include "runtime/GameComponents.hpp"
#include <iostream>
#include <stdexcept>
using namespace azurerender;
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
void install(RuntimeLifecycle& runtime,const char* node,const char* type,Json data){auto envelope=runtimeComponentRegistry().defaults(type);envelope["data"].update(data);runtimeComponentRegistry().install(type,runtime.world(),runtime.entity(node),envelope);}
int main(){try{
    auto envelope=runtimeComponentRegistry().defaults("azure.rigid-body");
    require(envelope.at("data").contains("shape"),"Rigid bodies must expose configurable collision shapes");
    RuntimeLifecycle runtime;SceneDocument scene;
    for(const auto* id:{"ground","ball","capsule","arm","anchor","hinge"}){SceneNode node;node.id=id;node.translation={0,4,0};scene.nodes.push_back(node);}
    scene.nodes[0].translation={0,-1,0};scene.nodes[4].translation={0,4,0};scene.nodes[3].translation={1,4,0};runtime.loadScene(scene);runtime.start();
    install(runtime,"ground","azure.rigid-body",{{"halfExtent",{10,1,10}}});
    install(runtime,"ball","azure.rigid-body",{{"shape","sphere"},{"radius",.4},{"dynamic",true}});
    runtime.world().tryGet<ecs::TransformComponent>(runtime.entity("ball"))->translation[0]=-3;
    install(runtime,"capsule","azure.rigid-body",{{"shape","capsule"},{"radius",.3},{"halfHeight",.6},{"dynamic",true}});
    runtime.world().tryGet<ecs::TransformComponent>(runtime.entity("capsule"))->translation[0]=3;
    install(runtime,"anchor","azure.rigid-body",{{"halfExtent",{.1,.1,.1}}});
    install(runtime,"arm","azure.rigid-body",{{"halfExtent",{1,.1,.1}},{"dynamic",true}});
    install(runtime,"hinge","azure.hinge",{{"bodyA","anchor"},{"bodyB","arm"},{"anchorA",{0,0,0}},{"anchorB",{-1,0,0}},{"axis",{0,0,1}},{"normal",{1,0,0}},{"minimumAngle",-60},{"maximumAngle",60}});
    PhysicsWorld physics;
    for(int i=0;i<180;++i)physics.step(runtime,1.F/60,{});
    const auto ball=runtime.world().tryGet<ecs::TransformComponent>(runtime.entity("ball"))->translation;
    const auto capsule=runtime.world().tryGet<ecs::TransformComponent>(runtime.entity("capsule"))->translation;
    const auto arm=runtime.world().tryGet<ecs::TransformComponent>(runtime.entity("arm"))->translation;
    std::cout<<"ball="<<ball[1]<<" capsule="<<capsule[1]<<" arm="<<arm[1]<<'\n';
    require(ball[1]>.35F&&ball[1]<.5F,"Sphere lands on floor");
    require(capsule[1]>.8F&&capsule[1]<1.F,"Capsule lands on floor");
    require(arm[1]>2.9F&&arm[1]<4.1F,"Hinge keeps arm attached");
    runtime.world().destroyEntity(runtime.entity("anchor"));physics.step(runtime,1.F/60,{});
    require(physics.constraintCount()==0,"Deleted endpoint releases its constraint before the body");
    install(runtime,"ground","azure.rigid-body",{{"shape","mesh"},{"mesh","fixture-mesh"}});
    auto* floor=runtime.world().tryGet<ecs::TransformComponent>(runtime.entity("ground"));floor->translation={20,0,0};floor->scale={-1,1,1};
    physics.setMeshResolver([](const std::string& reference){require(reference=="fixture-mesh","Mesh resolver receives a content reference");return CollisionMesh{{{{0,0,0},{0,0,2},{2,0,0}}}};});
    physics.step(runtime,1.F/60,{});const auto meshHit=physics.raycast({19.5F,5,.5F},{0,-10,0});
    require(meshHit&&meshHit->entity==runtime.entity("ground")&&std::abs(meshHit->fraction-.5F)<.001F,"Reflected static mesh uses actual triangles with corrected winding");
    auto invalidBody=runtimeComponentRegistry().defaults("azure.rigid-body");invalidBody["data"].update({{"shape","mesh"},{"mesh","fixture-mesh"},{"dynamic",true}});
    bool invalidMesh=false;try{runtimeComponentRegistry().validate("azure.rigid-body",invalidBody);}catch(const std::exception&){invalidMesh=true;}
    require(invalidMesh,"Dynamic triangle meshes are rejected before Jolt");
    RuntimeLifecycle door;SceneDocument mechanism;
    for(const auto* id:{"frame","panel","joint"}){SceneNode n;n.id=id;n.translation={0,2,0};mechanism.nodes.push_back(n);}
    mechanism.nodes[1].translation={1,2,0};door.loadScene(mechanism);door.start();
    install(door,"frame","azure.rigid-body",{{"halfExtent",{.1,1,.1}}});
    install(door,"panel","azure.rigid-body",{{"halfExtent",{1,1,.1}},{"dynamic",true}});
    install(door,"joint","azure.hinge",{{"bodyA","frame"},{"bodyB","panel"},{"anchorB",{-1,0,0}},{"minimumAngle",-15},{"maximumAngle",100}});
    PhysicsWorld doorPhysics;for(int i=0;i<180;++i)doorPhysics.step(door,1.F/60,{});
    const auto panel=door.world().tryGet<ecs::TransformComponent>(door.entity("panel"))->translation;
    require(doorPhysics.constraintCount()==1&&std::abs(panel[1]-2)<.03F,"Vertical-axis door remains attached using the shared hinge contract");
    const auto angle=doorPhysics.hingeAngle(door.entity("joint"));require(angle&&*angle>=-15.1F&&*angle<=100.1F,"Door hinge respects angle limits");
    auto bad=runtimeComponentRegistry().defaults("azure.hinge");bad["data"]["axis"]={0,0,0};
    bool rejected=false;try{runtimeComponentRegistry().validate("azure.hinge",bad);}catch(const std::exception&){rejected=true;}
    require(rejected,"Invalid axis must be rejected before authoring");
    std::cout<<"Sphere, capsule, reflected mesh, arm/door hinges, endpoint deletion and authoring validation passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
