#include "validation/ValidationTransport.hpp"
#include <iostream>
#include <fstream>
#include <cstdlib>
using namespace azurerender;
int main(int argc,char** argv) { try {
    if(argc!=3)throw std::invalid_argument("Usage: probe endpoint.json address");
    const auto* token=std::getenv("AZURE_VALIDATION_TEST_TOKEN");
    ObservationRegistry observations;std::int64_t frame=0;bool running=true;
    observations.add("engine.frameCount",[&]{return ObservationValue(frame);});
    observations.add("engine.status",[]{return ObservationValue(std::string("running"));});
    ValidationCallbacks callbacks;callbacks.edit=[&](const auto&){running=false;return nlohmann::json(true);};
    ValidationService service(observations,callbacks);
    ValidationTransport transport(service,{argv[2],token?token:"",0});
    { std::ofstream file(argv[1]);file<<nlohmann::json{{"address","127.0.0.1"},{"port",transport.port()}}; }
    const auto limit=ValidationService::Clock::now()+std::chrono::seconds(20);
    while(running&&ValidationService::Clock::now()<limit) { service.pump(static_cast<std::uint64_t>(++frame));std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
    // Give the transport time to deliver the final response before stopping.
    std::this_thread::sleep_for(std::chrono::milliseconds(50));transport.stop();return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;} }
