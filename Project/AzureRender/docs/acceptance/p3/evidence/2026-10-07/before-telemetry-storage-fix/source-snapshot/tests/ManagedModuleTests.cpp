#include "scripting/dotnet/ManagedModule.hpp"
#include "scripting/ScriptHostSession.hpp"
#include "app/ProjectRuntimeAssembly.hpp"
#include <filesystem>
#include <iostream>
#include <stdexcept>
using namespace azurerender;
using Json=nlohmann::json;
void require(bool v,const char* m){if(!v)throw std::runtime_error(m);}
int main(int argc,char** argv) try {
    if(argc!=3)throw std::invalid_argument("backend artifact-directory required");
    const std::string backend=argv[1];const std::filesystem::path directory=argv[2];
    RuntimeLifecycle runtime;SceneDocument scene;
    for(const char* id:{"hero","second"}){SceneNode node;node.id=id;scene.nodes.push_back(node);}
    runtime.loadScene(scene);runtime.start();GameRuntime game(runtime,application::systems(),application::explorationConfiguration());
    auto host=std::make_shared<ScriptBindingHost>(runtime,game);ScriptHostSession session(host);
    std::string text;host->setUiHandler([&](std::string,std::string value){text=value;});
    ManagedModule module(backend,directory/(backend=="coreclr"?"Azure.Engine.dll":"Azure.Engine.Native.dll"),directory/"Azure.Engine.runtimeconfig.json");
    require(module.call(session.api(),{{"operation","handshake"},{"apiVersion",1}}).at("apiVersion")==1,"Managed handshake failed");
    bool version=false;try{module.call(session.api(),{{"operation","handshake"},{"apiVersion",99}});}catch(const std::exception&){version=true;}
    require(version,"Managed ABI version must reject");
    for(const bool wrongVersion:{false,true}) {
        auto invalid=session.api();
        if(wrongVersion)invalid.version=99;else invalid.size=sizeof(ScriptHostApi)-1;
        bool rejected=false;
        try{module.call(invalid,{{"operation","handshake"},{"apiVersion",1}});}catch(const std::exception&){rejected=true;}
        require(rejected,"Actual managed entry must reject incompatible host table version and size");
    }
    const auto hero=host->object("hero");
    auto prepare=[&](const std::string& id,const ScriptObject& object,const std::string& type){
        host->beginCallback(true);bool callbackActive=true;
        try{module.call(session.api(),{{"operation","prepare"},{"id",id},{"object",ScriptBindingHost::encode(object)},
            {"assembly",(directory/"Azure.Samples.dll").u8string()},{"type",type}});host->endCallback(true);callbackActive=false;host->commitInitialization();
            module.call(session.api(),{{"operation","activate"},{"id",id}});
        }catch(...){if(callbackActive)host->endCallback(false);else host->cancelInitialization();throw;}
    };
    prepare("hero",hero,"Azure.Samples.Counter");prepare("second",host->object("second"),"Azure.Samples.Counter");
    for(const char* id:{"hero","hero","second"}){host->beginCallback();module.call(session.api(),{{"operation","callback"},{"id",id},{"callback","update"},{"delta",0.016}});host->endCallback(true);}
    require(runtime.world().tryGet<ecs::TransformComponent>(hero.identity.entity)->translation[0]==2,"Managed object state and array binding failed");
    require(runtime.world().tryGet<ecs::TransformComponent>(runtime.entity("second"))->translation[0]==1,"Managed entities must keep independent state");
    require(text=="脚本状态：1","UTF-8 managed string ownership failed");
    bool failed=false;try{prepare("hero",hero,"Azure.Samples.BrokenInit");}catch(const std::exception&){failed=true;}
    require(failed && runtime.world().tryGet<ecs::TransformComponent>(hero.identity.entity)->translation[0]==2,"Failed managed init must preserve state");
    host->beginCallback();module.call(session.api(),{{"operation","callback"},{"id","hero"},{"callback","update"},{"delta",0.016}});host->endCallback(true);
    require(runtime.world().tryGet<ecs::TransformComponent>(hero.identity.entity)->translation[0]==3,"Failed candidate must preserve active script");
    module.call(session.api(),{{"operation","closeObject"},{"id","hero"}});
    module.call(session.api(),{{"operation","closeSession"}});
    const auto diagnostics=module.call(session.api(),{{"operation","diagnostics"}});
    require(diagnostics.at("sessions")==0 && diagnostics.at("objects")==0,"Managed close must release all session objects");
    if(backend=="coreclr")require(diagnostics.at("liveContexts")==0,"CoreCLR collectible contexts must unload after actual GC");
    const auto saved=session.api();session.close();
    bool expired=false;
    try{module.call(saved,{{"operation","prepare"},{"id","retired"},{"object",ScriptBindingHost::encode(hero)},
        {"assembly",(directory/"Azure.Samples.dll").u8string()},{"type","Azure.Samples.Counter"}});}catch(const std::exception&){expired=true;}
    require(expired && module.call(saved,{{"operation","diagnostics"}}).at("sessions")==0,"Expired saved ABI must reject without retaining managed session objects");
    std::cout<<Json{{"status","passed"},{"backend",backend},{"diagnostics",diagnostics},{"startupMs",module.startupMilliseconds()}}.dump()<<'\n';
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
