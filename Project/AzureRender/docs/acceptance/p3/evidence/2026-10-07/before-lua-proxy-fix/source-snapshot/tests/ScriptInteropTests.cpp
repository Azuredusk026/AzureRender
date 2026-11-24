#include "scripting/ScriptHostSession.hpp"
#include "app/ProjectRuntimeAssembly.hpp"
#include <iostream>
#include <stdexcept>
#include <thread>
using namespace azurerender;
using Json=nlohmann::json;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main() try {
    RuntimeLifecycle runtime;SceneDocument scene;SceneNode node;node.id="hero";scene.nodes.push_back(node);
    runtime.loadScene(scene);runtime.start();GameRuntime game(runtime,application::systems(),application::explorationConfiguration());
    auto host=std::make_shared<ScriptBindingHost>(runtime,game);ScriptHostSession session(host);
    const auto api=session.api();const auto object=host->object("hero");
    auto request=Json{{"apiVersion",1},{"object",ScriptBindingHost::encode(object)},
                     {"method","get"},{"arguments",{"azure.transform","translation"}}};
    auto call=[&](const Json& input) {
        const auto bytes=input.dump();std::vector<std::uint8_t> output(kScriptInteropByteLimit);std::uint32_t written=0;
        const auto status=api.invoke(api.session,reinterpret_cast<const std::uint8_t*>(bytes.data()),
            static_cast<std::uint32_t>(bytes.size()),output.data(),static_cast<std::uint32_t>(output.size()),&written);
        require(written<=output.size(),"Interop response exceeded its owned buffer");
        return std::make_pair(status,Json::parse(output.begin(),output.begin()+written));
    };
    const auto initial=call(request);
    require(initial.first==0 && initial.second.at("ok")==true && initial.second.at("value")==Json({0,0,0}),"Real callback must read the shared component host");
    host->beginCallback();request["method"]="set";request["arguments"]={"azure.transform","translation",{1,2,3}};
    const auto encoded=request.dump();std::uint8_t small[4]{};std::uint32_t written=0;
    require(api.invoke(api.session,reinterpret_cast<const std::uint8_t*>(encoded.data()),static_cast<std::uint32_t>(encoded.size()),small,4,&written)!=0,
        "Insufficient output ownership must reject before effects");
    require(runtime.world().tryGet<ecs::TransformComponent>(object.identity.entity)->translation[0]==0,"Rejected output capacity must preserve the component");
    require(call(request).first==0,"Sized interop buffers must execute the mutation");host->endCallback(true);
    require(runtime.world().tryGet<ecs::TransformComponent>(object.identity.entity)->translation[0]==1,"Interop mutation must reach the actual world");
    request["apiVersion"]=99;require(call(request).first!=0,"Invalid binding version must reject");
    request["apiVersion"]=1;request["method"]="get";request["arguments"]={"azure.transform","translation"};
    bool foreignRejected=false;std::thread worker([&]{foreignRejected=call(request).first!=0;});worker.join();
    require(foreignRejected,"Saved callbacks must reject a foreign owner thread");
    session.close();const auto expired=call(request);
    require(expired.first!=0 && expired.second.at("ok")==false,"Saved callbacks must reject after session shutdown");
    std::cout<<"Real script interop, output ownership, version, owner thread and expired session passed\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
