#include "app/ProjectRuntimeAssembly.hpp"
#include "runtime/AssetDatabase.hpp"
#include "runtime/GameRuntime.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace azurerender;
using Json=nlohmann::json;
void require(bool v,const char* m){if(!v)throw std::runtime_error(m);}
void write(const std::filesystem::path& path,const Json& data){std::ofstream(path)<<data.dump();}
int main(int argc,char** argv) try {
    if(argc!=3)throw std::invalid_argument("backend artifact-directory required");
    const std::string backend=argv[1];const auto artifacts=std::filesystem::absolute(argv[2]);
    const auto root=std::filesystem::temp_directory_path()/("azure-managed-"+backend+"-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Project::create(root,"Managed service");auto project=Project::load(root/"project.azureproject");
    project.mounts["compiled"]=artifacts;
    project.scriptingConfiguration={{"schemaVersion",1},{"backend",backend}};
    if(backend!="lua")project.scriptingConfiguration["module"]="compiled:/"+std::string(backend=="coreclr"?"Azure.Engine.dll":"Azure.Engine.Native.dll");
    if(backend=="coreclr")project.scriptingConfiguration["runtimeConfig"]="compiled:/Azure.Engine.runtimeconfig.json";
    auto manifest=Json{{"schemaVersion",1},{"type","Azure.Samples.Counter"},{"assembly","compiled:/Azure.Samples.dll"}};
    if(backend=="nativeaot")manifest.erase("assembly");
    const auto scriptPath=root/(backend=="lua"?"assets/control.lua":"assets/control.azscript");
    const auto reference=backend=="lua"?"assets:/control.lua":"assets:/control.azscript";
    auto script=[&](const Json& descriptor) {
        if(backend!="lua"){write(scriptPath,descriptor);return;}
        const auto type=descriptor.at("type").get<std::string>();
        std::ofstream output(scriptPath);
        if(type=="Azure.Samples.Counter")output<<R"(function init() self.counter=0;self:set('azure.transform','translation',{0,0,0}) end
function update(dt) self.counter=self.counter+1;self:set('azure.transform','translation',{self.counter,0,0});self:ui_text('status','脚本状态：'..self.counter) end
function trigger(other,entered) if entered then self:ui_text('event','触发：'..other) end end)";
        else if(type=="Azure.Samples.BrokenInit")output<<"function init() self:set('azure.transform','translation',{900,0,0});error('候选初始化失败') end";
        else if(type=="Azure.Samples.BrokenUpdate")output<<"function update(dt) error('运行回调失败') end";
        else output<<"function update(dt) self:set('azure.transform','translation',{4,5,6}) end";
    };
    script(manifest);
    AssetDatabase assets(project);assets.refresh();RuntimeLifecycle runtime;SceneDocument scene;SceneNode node;node.id="hero";scene.nodes.push_back(node);
    runtime.loadScene(scene);runtime.start();GameRuntime game(runtime,application::systems(),application::explorationConfiguration());
    auto entity=runtime.entity("hero");runtime.world().addComponent(entity,game::Script{reference,true});
    const auto start=std::chrono::steady_clock::now();
    auto scripts=application::scripts(project,runtime,game,assets);
    const auto startup=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
    require(scripts->capabilities().backend==backend && scripts->capabilities().trustedCompiledExtension==(backend!="lua"),"Registered service must report the selected backend");
    std::string text;scripts->setUiHandler([&](std::string,std::string value){text=std::move(value);});
    scripts->fixedStep(0.016);scripts->fixedStep(0.016);
    require(scripts->activeCount()==1 && runtime.world().tryGet<ecs::TransformComponent>(entity)->translation[0]==2,"Real service must drive component state");
    scripts->dispatch({entity,entity,true});require(text=="触发：hero","Managed service must dispatch the real physics event");
    manifest["type"]="Azure.Samples.BrokenInit";script(manifest);
    scripts->reloadChanged();scripts->fixedStep(0.016);
    require(scripts->activeCount()==1 && runtime.world().tryGet<ecs::TransformComponent>(entity)->translation[0]==3,"Failed or unsupported reload must preserve the active service");
    require(!scripts->errors().empty(),"Rejected reload requires observable diagnostics");
    manifest["type"]="Azure.Samples.SceneAnnotator";script(manifest);
    scripts->reloadChanged();scripts->fixedStep(0.016);
    require(runtime.world().tryGet<ecs::TransformComponent>(entity)->translation[0]==4,"Reload must keep valid execution");
    if(backend!="nativeaot")require(runtime.world().tryGet<ecs::TransformComponent>(entity)->translation[2]==6,"Reload must execute the newly loaded script type");
    require(scripts->capabilities().hotReload==(backend!="nativeaot"),"NativeAOT reload capability must remain explicit");
    runtime.world().destroyEntity(entity);scripts->fixedStep(0.016);require(scripts->activeCount()==0,"Destroyed objects must release managed service entries");
    runtime.replaceScene(scene);entity=runtime.entity("hero");runtime.world().addComponent(entity,game::Script{reference,true});
    scripts->fixedStep(0.016);require(scripts->activeCount()==1,"Replacement scene must establish fresh object identities");
    const auto benchmark=std::chrono::steady_clock::now();
    for(unsigned index=0;index<1000;++index)scripts->fixedStep(0.016);
    const auto elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-benchmark).count();
    require(runtime.world().tryGet<ecs::TransformComponent>(entity)->translation[2]==6,"Measured callbacks must keep their component effect");
    // A failing object must stop without disabling another object in the same service.
    auto broken=manifest;broken["type"]="Azure.Samples.BrokenUpdate";script(broken);
    const auto brokenPath=root/(backend=="lua"?"assets/broken.lua":"assets/broken.azscript");
    std::filesystem::copy_file(scriptPath,brokenPath);
    script(manifest);
    SceneNode faulty;faulty.id="faulty";runtime.deferSpawn(faulty);runtime.beginFrame(0);
    const auto faultyEntity=runtime.entity("faulty");
    runtime.world().addComponent(faultyEntity,game::Script{backend=="lua"?"assets:/broken.lua":"assets:/broken.azscript",true});
    const auto beforeErrors=scripts->errors().size();
    scripts->fixedStep(0.016);
    require(scripts->activeCount()==1 && scripts->errors().size()>beforeErrors,"Callback failure must isolate the faulty object and retain diagnostics");
    require(scripts->errors().back().find("运行回调失败")!=std::string::npos,"Real callback exception must cross the language boundary");
    runtime.world().tryGet<ecs::TransformComponent>(entity)->translation[2]=0;
    scripts->fixedStep(0.016);
    require(scripts->activeCount()==1 && runtime.world().tryGet<ecs::TransformComponent>(entity)->translation[2]==6,"Healthy objects must continue after another object's callback failure");
    scripts->shutdown();scripts->shutdown();require(scripts->activeCount()==0,"Managed shutdown must be idempotent");
    bool closed=false;try{scripts->fixedStep(0);}catch(const std::exception&){closed=true;}require(closed,"Closed service must reject scheduling");
    std::filesystem::remove_all(root);
    std::cout<<"Registered "<<backend<<" service, components, events, reload, scene lifetime and shutdown passed\n";
    std::cout<<Json{{"backend",backend},{"startupMs",startup},{"iterations",1000},{"stepMsPerIteration",elapsed/1000}}.dump()<<'\n';
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
