#include "runtime/RuntimeLifecycle.hpp"
#include <iostream>
#include <stdexcept>
using namespace azurerender;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
SceneDocument document(){
    SceneDocument value;value.sceneId="snapshot-owner";
    value.resources.push_back({"mesh","model","mesh.gltf"});
    SceneNode parent;parent.id="parent";parent.name="Authored parent";parent.resourceId="mesh";
    parent.translation={1,2,3};parent.prefabSource="original.prefab";
    SceneNode child;child.id="child";child.parentId="parent";child.translation={4,5,6};child.visible=false;
    child.instanceOf="instance";value.nodes={parent,child};
    SceneLight light;light.id="light";light.nodeId="parent";value.lights.push_back(light);
    return value;
}
void equal(const SceneDocument& expected,const SceneDocument& actual){
    require(expected.sceneId==actual.sceneId&&expected.resources.size()==actual.resources.size(),"Snapshot metadata differs");
    require(expected.nodes.size()==actual.nodes.size()&&expected.lights.size()==actual.lights.size(),"Snapshot membership differs");
    for(std::size_t i=0;i<expected.nodes.size();++i){const auto& a=expected.nodes[i];const auto& b=actual.nodes[i];
        require(a.id==b.id&&a.name==b.name&&a.parentId==b.parentId&&a.resourceId==b.resourceId,"Node identity or static fields differ");
        require(a.translation==b.translation&&a.rotation==b.rotation&&a.scale==b.scale&&a.visible==b.visible,"Node dynamic fields differ");
        require(a.prefabSource==b.prefabSource&&a.instanceOf==b.instanceOf&&a.materialSlots==b.materialSlots,"Authoring metadata differs");
        require(a.objectShadow.has_value()==b.objectShadow.has_value(),"Shadow presence differs");
        if(a.objectShadow)require(a.objectShadow->center==b.objectShadow->center&&a.objectShadow->enabled==b.objectShadow->enabled,"Shadow value differs");
    }
    for(std::size_t i=0;i<expected.lights.size();++i)require(expected.lights[i].id==actual.lights[i].id,"Light membership differs");
}
}
int main(){try{
    RuntimeLifecycle runtime;runtime.loadScene(document());runtime.start();
    RuntimeSceneSnapshotCache cache;SceneDocument first,second;
    runtime.snapshotScene(first,cache);equal(runtime.snapshotScene(),first);
    const auto parent=runtime.entity("parent"),child=runtime.entity("child");
    auto& transform=*runtime.world().tryGet<ecs::TransformComponent>(child);transform.translation={8,9,10};
    runtime.world().tryGet<ecs::RenderableComponent>(child)->visible=true;
    runtime.world().addComponent(child,ecs::ObjectShadowComponent{});
    runtime.snapshotScene(first,cache);equal(runtime.snapshotScene(),first);
    const auto owned=runtime.snapshotScene();
    runtime.world().componentArray<ecs::TransformComponent>().erase(child);
    runtime.world().componentArray<ecs::RenderableComponent>().erase(child);
    runtime.world().componentArray<ecs::ObjectShadowComponent>().erase(child);
    runtime.snapshotScene(first,cache);equal(runtime.snapshotScene(),first);
    require(first.nodes[1].translation==std::array<float,3>{4,5,6}&&!first.nodes[1].visible,"Missing components must restore authored defaults");
    require(owned.nodes[1].translation==std::array<float,3>{8,9,10},"Owned snapshots must remain independent");
    cache.exchange(first,second);runtime.snapshotScene(first,cache);equal(runtime.snapshotScene(),first);
    cache.exchange(first,second);runtime.snapshotScene(first,cache);equal(runtime.snapshotScene(),first);
    const auto oldSource=cache.sourceIdentity(first);
    SceneNode spawned;spawned.id="spawned";spawned.parentId="child";runtime.deferSpawn(spawned);runtime.beginFrame(0);
    runtime.snapshotScene(first,cache);equal(runtime.snapshotScene(),first);
    require(cache.sourceIdentity(first)!=oldSource&&first.nodes.size()==3,"Spawn must renew the immutable source identity");
    runtime.world().destroyEntity(parent);const auto reused=runtime.world().createEntity();require(reused==parent,"Entity reuse control missing");
    runtime.world().addComponent(reused,ecs::TransformComponent{});
    runtime.snapshotScene(first,cache);equal(runtime.snapshotScene(),first);
    require(first.nodes[0].id=="child"&&first.nodes[0].parentId.empty()&&first.lights.empty(),"Stale parents and light nodes must be removed");
    auto replacement=document();replacement.sceneId="replaced";replacement.nodes[0].name="Replacement";
    runtime.replaceScene(replacement);runtime.snapshotScene(first,cache);equal(runtime.snapshotScene(),first);
    require(first.nodes[0].name=="Replacement","Same node identities must refresh static data after replacement");
    RuntimeLifecycle other;auto otherDocument=document();otherDocument.sceneId="other-runtime";other.loadScene(otherDocument);
    other.snapshotScene(first,cache);equal(other.snapshotScene(),first);
    require(first.sceneId=="other-runtime","Snapshot cache must distinguish runtime owners");
    other.world().clear();other.world().createEntity();other.snapshotScene(first,cache);equal(other.snapshotScene(),first);
    require(first.nodes.empty(),"World clearing must invalidate captured entity generations");
    cache.clear();runtime.snapshotScene(first,cache);equal(runtime.snapshotScene(),first);
    std::cout<<"Runtime scene snapshot cache contracts passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
