#define NOMINMAX
#include <windows.h>
#include <psapi.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <cstdint>
std::size_t memory(){PROCESS_MEMORY_COUNTERS_EX c{};c.cb=sizeof(c);K32GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&c),sizeof(c));return c.PrivateUsage;}
int main(){
 nlohmann::json samples=nlohmann::json::array();
 auto record=[&](std::uint64_t frame){samples.push_back({{"frame",frame},{"revision",frame/10},{"workMs",1.2},{"waitMs",.2},{"physicsMaxMs",.3},{"committed",false},{"commitMs",1.0},{"loading",false},{"cachedCandidates",2},{"requestGeneration",frame/10},{"loadError",""},{"buffers",49},{"images",71},{"bufferBytes",7470152},{"imageBytes",124575232}});};
 for(std::uint64_t i=0;i<260;++i)record(i);
 auto baseline=memory();
 for(std::uint64_t i=260;i<3576;++i)record(i);
 auto actual=memory();std::cout<<"Legacy retained JSON telemetry private bytes: "<<baseline<<" -> "<<actual<<"; growth "<<actual-baseline<<"; records "<<samples.size()<<'\n';
}
