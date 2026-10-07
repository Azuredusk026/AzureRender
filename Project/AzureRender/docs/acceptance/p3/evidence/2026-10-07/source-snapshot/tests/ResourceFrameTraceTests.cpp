#include "runtime/ResourceFrameTrace.hpp"
#include <iostream>
#include <sstream>
#include <stdexcept>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <psapi.h>
std::size_t privateBytes(){
    PROCESS_MEMORY_COUNTERS_EX counters{};counters.cb=sizeof(counters);
    if(!K32GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),sizeof(counters)))
        throw std::runtime_error("Cannot observe trace process memory");
    return counters.PrivateUsage;
}
#endif
using namespace azurerender;
void check(bool value){if(!value)throw std::runtime_error("Resource trace contract failed");}
int main(){try{
    ResourceFrameTrace trace(3);
    ResourceFrameSample sample;
    sample.revision=7;sample.workMs=1.5;sample.waitMs=.25;sample.physicsMaxMs=.75;
    sample.committed=true;sample.commitMs=2;sample.loading=true;sample.cachedCandidates=2;
    sample.requestGeneration=9;sample.loadError="candidate failed";sample.buffers=49;sample.images=71;
    sample.bufferBytes=7470152;sample.imageBytes=124575232;
    for(std::uint64_t i=1;i<=5;++i){sample.frame=i;trace.record(sample);}
    std::ostringstream output;trace.write(output);auto data=nlohmann::json::parse(output.str());
    check(data.size()==3 && data[0]["frame"]==3 && data[2]["frame"]==5);
    auto expected=nlohmann::json{{"frame",3},{"revision",7},{"workMs",1.5},{"waitMs",.25},
        {"physicsMaxMs",.75},{"committed",true},{"commitMs",2},{"loading",true},
        {"cachedCandidates",2},{"requestGeneration",9},{"loadError","candidate failed"},
        {"buffers",49},{"images",71},{"bufferBytes",7470152},{"imageBytes",124575232}};
    check(data[0]==expected);
    bool zero=false,large=false;
    try{ResourceFrameTrace invalid(0);}catch(const std::invalid_argument&){zero=true;}
    try{ResourceFrameTrace invalid(8193);}catch(const std::invalid_argument&){large=true;}
    check(zero&&large);
    ResourceFrameTrace empty(1);std::ostringstream blank;empty.write(blank);check(blank.str()=="[]");
#ifdef _WIN32
    const auto before=privateBytes();
    ResourceFrameTrace bounded;
    ResourceFrameSample normal;normal.buffers=49;normal.images=71;
    for(std::uint64_t i=0;i<8192;++i){normal.frame=i;bounded.record(normal);}
    const auto after=privateBytes();
    std::cout<<"Retained resource trace private bytes: "<<before<<" -> "<<after<<'\n';
    if(after>before+2*1024*1024)throw std::runtime_error("Resource telemetry retained more than two MiB");
    for(std::uint64_t i=8192;i<10000;++i){normal.frame=i;bounded.record(normal);}
    std::ostringstream wrapped;bounded.write(wrapped);auto rows=nlohmann::json::parse(wrapped.str());
    check(rows.size()==8192 && rows.front()["frame"]==1808 && rows.back()["frame"]==9999);
#endif
    std::cout<<"Trace schema, bounded ordered retention and memory budgets passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
