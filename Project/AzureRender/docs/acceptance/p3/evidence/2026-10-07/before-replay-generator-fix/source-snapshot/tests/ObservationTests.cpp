#include "runtime/ObservationRegistry.hpp"
#include "validation/ValidationService.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace azurerender;
using Json=nlohmann::json;
void check(bool value,const char* message) { if(!value)throw std::runtime_error(message); }
template<class F> void rejects(F f) { bool rejected=false;try { f(); }catch(const std::exception&){rejected=true;}check(rejected,"Expected rejection"); }
int main() { try {
    ObservationRegistry observations;std::int64_t count=0;
    observations.add("engine.frameCount",[&] { return ObservationValue(count); });
    observations.add("ready",[&] { return ObservationValue(count>=3); });
    observations.add("selection.id",[] { return ObservationValue(std::string("stable-node")); });
    observations.add("fraction",[] { return ObservationValue(0.25); });
    rejects([&]{observations.add("ready",[]{return ObservationValue(true);});});
    rejects([&]{observations.query("missing");});
    observations.add("nan",[]{return ObservationValue(std::numeric_limits<double>::infinity());});
    rejects([&]{observations.query("nan");});
    check(observations.names().size()==5,"Discover registered values");
    check(observations.query("selection.id")=="stable-node","Stable identity");
    int writes=0,inputs=0,captures=0;
    ValidationCallbacks callbacks;
    callbacks.edit=[&](const Json& value){++writes;return value;};
    callbacks.input=[&](const Json& value){++inputs;return value;};
    callbacks.capture=[&](const Json& value){++captures;return value;};
    ValidationService service(observations,callbacks);
    auto request=[&](Json command){command["schemaVersion"]=1;return service.submit(command);};
    auto wait=request({{"op","wait-until"},{"name","ready"},{"equals",true},{"timeoutMs",1000}});
    service.pump(0);check(wait.wait_for(std::chrono::milliseconds(0))!=std::future_status::ready,"Conditional wait is asynchronous");
    count=3;service.pump(1);check(wait.get().at("passed"),"Wait observes changing state");
    auto missing=request({{"op","query"},{"name","unknown"}});service.pump(2);check(!missing.get().at("passed"),"Unknown query diagnostic");
    auto fail=request({{"op","assert"},{"name","ready"},{"equals",false}});service.pump(3);check(!fail.get().at("passed"),"Assert failure");
    auto timeout=request({{"op","wait-until"},{"name","ready"},{"equals",false},{"timeoutMs",1}});
    service.pump(4);service.pump(5,ValidationService::Clock::now()+std::chrono::seconds(1));
    check(timeout.get().at("code")=="Timeout","Finite wait timeout");
    auto frames=request({{"op","wait-frames"},{"frames",2}});service.pump(6);service.pump(7);
    check(frames.wait_for(std::chrono::milliseconds(0))!=std::future_status::ready,"Frame wait honors count");
    service.pump(8);check(frames.get().at("passed"),"Frame wait completes");
    for(const char* op:{"edit","input","screenshot"}) { auto result=request({{"op",op}});service.pump(9);check(result.get().at("passed"),"Production callback"); }
    check(writes==1&&inputs==1&&captures==1,"Dispatch exactly once");
    auto oversized=request({{"op","query"},{"text",std::string(70000,'x')}});check(!oversized.get().at("passed"),"Request size budget");
    auto fractional=service.submit({{"schemaVersion",1.0},{"op","query"},{"name","ready"}});service.pump(10);check(!fractional.get().at("passed"),"Protocol version is an integer");
    auto scriptService=ValidationService(observations);
    scriptService.loadScript({{"schemaVersion",1},{"steps",Json::array({{{"op","assert"},{"name","ready"},{"equals",false}},{{"op","query"},{"name","ready"}}})}});
    scriptService.pump(0);check(scriptService.complete()&&!scriptService.report().at("passed")&&scriptService.report().at("completedSteps")==1,"Script failure stops later operations");
    rejects([&]{scriptService.loadScript({{"schemaVersion",1},{"steps",Json(std::vector<Json>(1025,Json::object()))}});});
    std::vector<std::future<Json>> queue;
    for(int i=0;i<65;++i)queue.push_back(request({{"op","wait-until"},{"name","ready"},{"equals",false}}));
    check(queue.back().get().at("code")=="QueueFull","Queue budget");
    service.stop();for(std::size_t i=0;i+1<queue.size();++i)check(queue[i].get().at("code")=="Stopped","Shutdown completes all requests");
    auto stopped=request({{"op","query"},{"name","ready"}});check(!stopped.get().at("passed"),"Closed service rejects submissions");
    std::cout<<"Observation and validation contracts passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;} }
