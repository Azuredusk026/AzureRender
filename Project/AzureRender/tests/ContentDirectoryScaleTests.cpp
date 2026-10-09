#include "editor/content/ContentDirectoryModel.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <psapi.h>
#pragma comment(lib,"psapi.lib")
#endif
using namespace azurerender;
int main(){
    const auto root=std::filesystem::temp_directory_path()/("azure-directory-scale-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));int status=0;
    try{
        Project::create(root,"Scale");std::filesystem::create_directory(root/"assets/large");
        for(int i=0;i<10000;++i)std::ofstream(root/("assets/large/asset-"+std::to_string(i)+".txt"))<<i;
        auto context=EditorContext::openProject(root/"project.azureproject");ContentDirectoryModel model;
        auto start=std::chrono::steady_clock::now();const auto snapshot=model.snapshot(*context,"assets","large","text");
        const auto enumeration=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
        if(snapshot->assets.size()!=10000)throw std::runtime_error("Large directory must expose every asset");
        const auto initial=model.statistics();std::vector<double> elapsed;
        for(int i=0;i<2100;++i){start=std::chrono::steady_clock::now();const auto cached=model.snapshot(*context,"assets","large","text");const auto time=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();if(i>=300)elapsed.push_back(time);if(cached.get()!=snapshot.get())throw std::runtime_error("Unchanged directory must retain its immutable snapshot");}
        std::sort(elapsed.begin(),elapsed.end());const auto p95=elapsed[static_cast<std::size_t>(elapsed.size()*.95)];
        if(p95>10)throw std::runtime_error("Cached directory work exceeds the 10ms editor CPU budget");
        const auto copied=context->assets();const auto originalCount=context->assets().records().size();
        const auto after=model.statistics();if(after.at("catalogBuilds")!=initial.at("catalogBuilds")||after.at("directoryEnumerations")!=initial.at("directoryEnumerations"))throw std::runtime_error("Cached frames must not enumerate files");
        const auto selected=model.snapshot(*context,"assets","large","text","asset-1234.txt");if(selected->assets.size()!=1)throw std::runtime_error("Filtered snapshots must retain exact matching identity");
        std::filesystem::create_directory(root/"assets/external");std::ofstream(root/"assets/external/new.txt")<<"external";
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(30);bool discovered=false;
        while(std::chrono::steady_clock::now()<deadline){model.poll(*context);try{const auto result=model.snapshot(*context,"assets","external","text");if(result->assets.size()==1){discovered=true;break;}}catch(const std::exception&){}std::this_thread::sleep_for(std::chrono::milliseconds(20));}
        if(!discovered)throw std::runtime_error("External changes must publish without explicit reload");
        if(copied.records().size()!=originalCount)throw std::runtime_error("Shared immutable source caches must preserve database snapshot isolation");
        std::filesystem::create_directory(root/"assets/cancelled");std::ofstream(root/"assets/cancelled/new.txt")<<"cancel";
        std::this_thread::sleep_for(std::chrono::milliseconds(320));model.poll(*context);
        if(!model.statistics().at("refreshing").get<bool>())throw std::runtime_error("External change must start the cancellable scan");
        start=std::chrono::steady_clock::now();model.reset();const auto resetMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
        std::cout<<nlohmann::json{{"directoryResetMs",resetMs}}.dump()<<std::endl;
        if(resetMs>100)throw std::runtime_error("Directory teardown must cancel in at most 100ms");
        std::size_t peakBytes=0;
#ifdef _WIN32
        PROCESS_MEMORY_COUNTERS memory{};memory.cb=sizeof(memory);if(GetProcessMemoryInfo(GetCurrentProcess(),&memory,sizeof(memory)))peakBytes=memory.PeakWorkingSetSize;
#endif
        std::cout<<nlohmann::json{{"assets",10000},{"warmup",300},{"samples",1800},{"firstSnapshotMs",enumeration},{"cachedP95Ms",p95},{"peakWorkingSetBytes",peakBytes},{"statistics",model.statistics()}}.dump()<<'\n';
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';status=1;}
    std::filesystem::remove_all(root);return status;
}
