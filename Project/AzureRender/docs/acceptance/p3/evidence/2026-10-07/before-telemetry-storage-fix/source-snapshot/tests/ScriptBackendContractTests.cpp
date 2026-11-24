#include "scripting/ScriptBindingHost.hpp"
#include "app/ProjectRuntimeAssembly.hpp"
#include "runtime/ComponentCodec.hpp"
#include <iostream>
#include <stdexcept>
using namespace azurerender;
using Json=nlohmann::json;
namespace {
void check(bool value,const char* message) { if(!value)throw std::runtime_error(message); }
template<class F> void rejects(F call,const char* message) {
    bool rejected=false;try{call();}catch(const std::exception&){rejected=true;}
    check(rejected,message);
}
void entityLifetimes() {
    ecs::World world;
    const auto id=world.createEntity();const auto original=world.handle(id);
    check(world.valid(original),"Fresh entity handle must be valid");
    world.destroyEntity(id);const auto reused=world.createEntity();
    check(id==reused && !world.valid(original),"Numeric reuse must invalidate the original handle");
    const auto second=world.handle(reused);world.clear();world.createEntity();
    check(!world.valid(second),"Clear and rebuild must invalidate the original identity");
    const auto current=world.handle(id);ecs::World other;other.createEntity();
    check(!other.valid(current),"Another world cannot accept a foreign entity identity");
    world.swap(other);
    check(!world.valid(current) && other.valid(current),"Swap must transfer identities with their entities");
}
void bindings() {
    RuntimeLifecycle runtime;SceneDocument scene;
    for(const auto* id:{"hero","target"}){SceneNode node;node.id=id;scene.nodes.push_back(node);}
    runtime.loadScene(scene);runtime.start();
    GameRuntime game(runtime,application::systems(),application::explorationConfiguration());
    ScriptBindingHost host(runtime,game);
    const auto hero=host.object("hero"),target=host.object("target");
    check(host.invoke(hero,"id",Json::array())=="hero","Object identity must be readable");
    rejects([&]{host.invoke(hero,"set",{"azure.transform","translation",{1,2,3}});},"Mutation outside a callback must fail");
    rejects([&]{host.invoke(hero,"find",{true});},"Wrong binding parameter type must fail");
    rejects([&]{host.invoke(hero,"id",{"other"});},"Object identity must be read only");
    host.beginCallback(true);
    host.invoke(target,"set",{"azure.transform","translation",{2,3,4}});
    check(host.invoke(target,"get",{"azure.transform","translation"})==Json({2,3,4}),"Initialization must read its staged value");
    host.endCallback(false);
    check(runtime.world().tryGet<ecs::TransformComponent>(runtime.entity("target"))->translation[0]==0,"Failed initialization must isolate cross-entity properties");
    host.beginCallback(true);host.invoke(target,"set",{"azure.transform","translation",{2,3,4}});
    host.endCallback(true);host.commitInitialization();
    check(runtime.world().tryGet<ecs::TransformComponent>(runtime.entity("target"))->translation[0]==2,"Successful initialization must commit staged properties");
    unsigned deliveries=0;host.setUiHandler([&](std::string,std::string){++deliveries;throw std::runtime_error("Host delivery failed");});
    host.beginCallback(true);host.invoke(hero,"ui_text",{"status","初始化消息"});host.endCallback(true);
    host.commitInitialization();
    check(deliveries==0,"Initialization host events must be delivered at the committed runtime boundary");
    rejects([&]{runtime.beginFrame(0);},"Host delivery failures must be observable at the runtime boundary");
    check(deliveries==1,"Committed host event must execute exactly once");
    host.beginCallback(true);host.invoke(hero,"ui_text",{"status","取消消息"});host.endCallback(false);runtime.beginFrame(0);
    check(deliveries==1,"Failed candidate must cancel staged host effects");
    host.beginCallback(true);host.invoke(target,"set",{"azure.transform","translation",{77,0,0}});
    host.invoke(hero,"spawn",{"candidate-before-conflict","",{0,0,0}});
    host.invoke(hero,"spawn",{"reserved-conflict","",{0,0,0}});host.endCallback(true);
    SceneNode reserved;reserved.id="reserved-conflict";runtime.deferSpawn(reserved);
    rejects([&]{host.commitInitialization();},"A reservation conflict must reject candidate commit");
    check(runtime.world().tryGet<ecs::TransformComponent>(target.identity.entity)->translation[0]==2,"Commit failure must restore installed properties");
    runtime.beginFrame(0);
    check(runtime.entity("reserved-conflict")!=ecs::kInvalidEntity && runtime.entity("candidate-before-conflict")==ecs::kInvalidEntity,
        "Failed commit must restore the original queue and spawn reservations");
    RuntimeLifecycle replacement;replacement.loadScene(scene);runtime.world().swap(replacement.world());
    check(!host.valid(target),"Same scene and same node name cannot revive an old object handle");
    rejects([&]{host.invoke(target,"get",{"azure.transform","translation"});},"World replacement must reject the saved object");
    const auto fresh=host.object("target");host.close();
    check(!host.valid(fresh),"Closing the binding host must invalidate all session objects");
}
}
int main() try {
    entityLifetimes();bindings();
    std::cout<<"Script entity identities, binding permissions and shared initialization transactions passed\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
