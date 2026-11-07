#include "runtime/PreviousTransformSnapshot.hpp"
#include "runtime/RuntimeLifecycle.hpp"
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>
using namespace azurerender;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
SceneDocument document(std::string prefix="node"){
 SceneDocument d;d.sceneId=prefix;
 for(unsigned i=0;i<3;++i){SceneNode n;n.id=prefix+std::to_string(i);n.translation={float(i),-float(i),.125F};n.rotation={10.F*float(i),350.F,0};d.nodes.push_back(n);}return d;
}
void compare(PreviousTransformSnapshot& cached,RuntimeLifecycle& runtime){
 std::map<std::string,ecs::TransformComponent> expected;
 runtime.world().each<ecs::TransformComponent>([&](auto e,const auto& t){expected[runtime.nodeId(e)]=t;});cached.capture(runtime);
 for(const auto& [node,t]:expected){const auto* found=cached.find(node);require(found&&std::memcmp(found,&t,sizeof(t))==0,"Captured transform bytes differ from complete traversal");}
 require(!cached.find("missing"),"Absent node lookup must remain empty");
}
}
int main()try{
 RuntimeLifecycle runtime;runtime.loadScene(document());runtime.start();PreviousTransformSnapshot cache;
 compare(cache,runtime);require(cache.rebuilt(),"First use builds node identities");compare(cache,runtime);require(!cache.rebuilt(),"Stable layout reuses identity storage");
 const auto entity=runtime.entity("node0");const auto previous=*cache.find("node0");auto* t=runtime.world().tryGet<ecs::TransformComponent>(entity);t->translation={-0.F,7,8};
 require(std::memcmp(cache.find("node0"),&previous,sizeof(previous))==0,"History owns values until the next fixed step");compare(cache,runtime);require(!cache.rebuilt(),"Value edits retain the identity layout");
 runtime.world().componentArray<ecs::TransformComponent>().erase(entity);compare(cache,runtime);require(!cache.find("node0")&&cache.rebuilt(),"Removed components leave the history");
 runtime.world().addComponent(entity,ecs::TransformComponent{});compare(cache,runtime);require(cache.rebuilt(),"Reinserted component order rebuilds identities");
 const auto removed=runtime.entity("node1");const auto oldHandle=runtime.world().handle(removed);runtime.world().destroyEntity(removed);const auto reused=runtime.world().createEntity();require(reused==removed,"Entity reuse control is required");
 runtime.world().addComponent(reused,ecs::TransformComponent{{9,8,7}});compare(cache,runtime);require(cache.rebuilt()&&!cache.find("node1")&&!runtime.world().valid(oldHandle),"Entity generations reject stale node identities at equal count");
 SceneNode spawned;spawned.id="spawned";runtime.deferSpawn(spawned);runtime.beginFrame(0);compare(cache,runtime);require(cache.find("spawned")&&cache.rebuilt(),"Spawned runtime nodes join the history");
 const auto anonymous=runtime.world().createEntity();runtime.world().addComponent(anonymous,ecs::TransformComponent{{4,5,6}});compare(cache,runtime);require(cache.find("")->translation[0]==4,"Anonymous traversal retains the final empty-key value");
 runtime.world().addComponent(999999,ecs::TransformComponent{{1,2,3}});compare(cache,runtime);compare(cache,runtime);require(!cache.rebuilt()&&cache.find("")->translation[0]==1,"Invalid component entities preserve complete traversal behavior");
 PreviousTransformSnapshot bounded(1);compare(bounded,runtime);require(bounded.find("spawned"),"Oversized layouts preserve complete lookup behavior");
 runtime.pause();compare(cache,runtime);require(!cache.rebuilt(),"Pause retains captured identity storage");runtime.resume();
 runtime.replaceScene(document("other"));compare(cache,runtime);require(cache.rebuilt()&&!cache.find("node0")&&cache.find("other0"),"Scene replacement starts an independent identity epoch");
 RuntimeLifecycle second;second.loadScene(document("other"));compare(cache,second);require(cache.rebuilt(),"Runtime ownership participates in invalidation");
 second.world().clear();compare(cache,second);require(!cache.find("other0"),"An empty world owns empty history");cache.clear();require(!cache.find("other1"),"Explicit clear releases indexed history");compare(cache,runtime);require(cache.rebuilt(),"Cleared storage can capture a live runtime");
 std::cout<<"Previous transforms: owned values, topology, generations, spawn, missing components, fallback and scene epochs passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
