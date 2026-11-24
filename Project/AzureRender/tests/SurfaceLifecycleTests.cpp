#include "platform/SurfaceLifecycle.hpp"
#include "validation/ValidationService.hpp"
#include "editor/documents/EditorTaskService.hpp"

#include <iostream>
#include <stdexcept>
#include <utility>

struct Frontend {
    std::pair<int, int> size{1280, 720};
    int waits = 0;
    int timedWaits = 0;
    bool closed = false;
    bool closeOnWait = false;
    int restoreAfterWaits = 1;
    auto framebufferSize() const { return size; }
    bool shouldClose() const { return closed; }
    void waitEvents() {
        if (++waits > 10) throw std::runtime_error("waiting indefinitely");
        if (closeOnWait) closed = true;
        else if(waits>=restoreAfterWaits)size = {1280, 720};
    }
    void waitEventsTimeout(double seconds) {
        if(!(seconds>0&&seconds<=.05))throw std::runtime_error("surface event wait exceeds its service interval");
        ++timedWaits;waitEvents();
    }
};
void require(bool value,const char* message) {if(!value)throw std::runtime_error(message);}

void idle_services() {
    using namespace azurerender;
    using Json=nlohmann::json;
    Frontend frontend;frontend.size={0,0};frontend.restoreAfterWaits=4;
    ObservationRegistry observations;
    observations.add("engine.frameCount",[]{return ObservationValue(std::int64_t{17});});
    ValidationService validation(observations);
    auto query=validation.submit({{"schemaVersion",1},{"op","query"},{"name","engine.frameCount"}});
    auto frames=validation.submit({{"schemaVersion",1},{"op","wait-frames"},{"frames",1},{"timeoutMs",1000}});
    auto timeout=validation.submit({{"schemaVersion",1},{"op","wait-frames"},{"frames",1},{"timeoutMs",100}});
    bool committed=false;
    EditorTaskService tasks([](const auto&,const auto&){throw std::runtime_error("Unexpected editor task failure");});
    tasks.add("surface.fixture","document",{
        []{return .5F;},[&]{return frontend.waits>=2;},[&]{committed=true;return Json{{"applied",true}};},[]{}});
    const auto start=ValidationService::Clock::now();
    const auto drawable=waitForDrawableSurface(frontend,[&] {
        validation.pump(17,start+std::chrono::milliseconds(frontend.waits*50));
        tasks.poll();return true;
    });
    require(drawable&&frontend.timedWaits==4,"Repeated zero-size events must use bounded waits until restoration");
    require(query.get().at("value")==17,"Production queries must complete while the surface is unavailable");
    require(committed&&tasks.report().at(0).at("state")=="completed","Ready editor tasks must commit without a rendered frame");
    require(timeout.get().at("code")=="Timeout","Frame requests retain wall-clock deadlines while rendering is paused");
    require(frames.wait_for(std::chrono::milliseconds(0))!=std::future_status::ready,
        "Idle service cycles must never satisfy a rendered-frame wait");
    validation.pump(18,start+std::chrono::milliseconds(200));
    require(frames.get().at("passed"),"Frame waits resume only after the actual host frame advances");
}

void close_during_idle() {
    Frontend frontend;frontend.size={0,0};unsigned pumps=0;
    require(!azurerender::waitForDrawableSurface(frontend,[&]{++pumps;return false;}),
        "A completed document-close operation must end surface waiting");
    require(pumps==1&&frontend.waits==0,"Host close must stop before another native event wait");
    Frontend closing;closing.size={0,0};
    require(!azurerender::waitForDrawableSurface(closing,[&]{closing.closed=true;return true;}),
        "A host service can request native window closure");
    require(closing.waits==0,"Native closure from idle work must stop immediately");
}

int main() {try {
    Frontend ready;
    if (!azurerender::waitForDrawableSurface(ready) || ready.waits != 0)
        return 1;
    Frontend minimized;
    minimized.size = {0, 0};
    if (!azurerender::waitForDrawableSurface(minimized) || minimized.waits != 1 || minimized.timedWaits != 1)
        return 2;
    Frontend closing;
    closing.size = {0, 0};
    closing.closeOnWait = true;
    if (azurerender::waitForDrawableSurface(closing) || closing.waits != 1)
        return 3;
    Frontend closed;
    closed.closed = true;
    if (azurerender::waitForDrawableSurface(closed) || closed.waits != 0)
        return 4;
    idle_services();close_during_idle();
    std::cout<<"Surface lifecycle: bounded waiting, production services, deadlines and closure passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
