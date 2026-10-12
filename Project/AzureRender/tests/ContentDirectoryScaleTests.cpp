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
    const auto began=std::chrono::steady_clock::now();
    const auto checkpoint=[&](const char* phase){std::cout<<nlohmann::json{{"phase",phase},{"elapsedMs",std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-began).count()}}.dump()<<std::endl;};
    const auto root=std::filesystem::temp_directory_path()/("azure-directory-scale-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));int status=0;
    try{
        Project::create(root,"Scale");std::filesystem::create_directory(root/"assets/large");
        for(int i=0;i<10000;++i)std::ofstream(root/("assets/large/asset-"+std::to_string(i)+".txt"))<<i;
        checkpoint("fixture-created");
        auto context=EditorContext::openProject(root/"project.azureproject");ContentDirectoryModel model;
        checkpoint("project-opened");
        auto start=std::chrono::steady_clock::now();const auto snapshot=model.snapshot(*context,"assets","large","text");
        const auto enumeration=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
        if(snapshot->assets.size()!=10000)throw std::runtime_error("Large directory must expose every asset");
        checkpoint("first-snapshot");
        const auto initial=model.statistics();std::vector<double> elapsed;
        for(int i=0;i<2100;++i){start=std::chrono::steady_clock::now();const auto cached=model.snapshot(*context,"assets","large","text");const auto time=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();if(i>=300)elapsed.push_back(time);if(cached.get()!=snapshot.get())throw std::runtime_error("Unchanged directory must retain its immutable snapshot");}
        std::sort(elapsed.begin(),elapsed.end());const auto p95=elapsed[static_cast<std::size_t>(elapsed.size()*.95)];
        if(p95>10)throw std::runtime_error("Cached directory work exceeds the 10ms editor CPU budget");
        checkpoint("cached-samples");
        const auto copied=context->assets();const auto originalCount=context->assets().records().size();
        const auto after=model.statistics();if(after.at("catalogBuilds")!=initial.at("catalogBuilds")||after.at("directoryEnumerations")!=initial.at("directoryEnumerations"))throw std::runtime_error("Cached frames must not enumerate files");
        const auto selected=model.snapshot(*context,"assets","large","text","asset-1234.txt");if(selected->assets.size()!=1)throw std::runtime_error("Filtered snapshots must retain exact matching identity");
        std::filesystem::create_directory(root/"assets/external");std::ofstream(root/"assets/external/new.txt")<<"external";
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(30);bool discovered=false;
        while(std::chrono::steady_clock::now()<deadline){model.poll(*context);try{const auto result=model.snapshot(*context,"assets","external","text");if(result->assets.size()==1){discovered=true;break;}}catch(const std::exception&){}std::this_thread::sleep_for(std::chrono::milliseconds(20));}
        if(!discovered)throw std::runtime_error("External changes must publish without explicit reload");
        checkpoint("external-refresh");
        if(copied.records().size()!=originalCount)throw std::runtime_error("Shared immutable source caches must preserve database snapshot isolation");
        std::filesystem::create_directory(root/"assets/cancelled");std::ofstream(root/"assets/cancelled/new.txt")<<"cancel";
        std::this_thread::sleep_for(std::chrono::milliseconds(320));model.poll(*context);
        if(!model.statistics().at("refreshing").get<bool>())throw std::runtime_error("External change must start the cancellable scan");
        start=std::chrono::steady_clock::now();model.cancelRefresh();const auto cancelledAt=std::chrono::steady_clock::now();model.reset();const auto resetAt=std::chrono::steady_clock::now();
        const auto resetMs=std::chrono::duration<double,std::milli>(resetAt-start).count();
        std::cout<<nlohmann::json{{"directoryResetMs",resetMs},{"scanCancellationMs",std::chrono::duration<double,std::milli>(cancelledAt-start).count()},{"snapshotReleaseMs",std::chrono::duration<double,std::milli>(resetAt-cancelledAt).count()}}.dump()<<std::endl;
        if(resetMs>100)throw std::runtime_error("Directory teardown must cancel in at most 100ms");
        for(int cycle=0;cycle<3;++cycle){
            checkpoint("reset-cycle-start");
            model.snapshot(*context,"assets","large","text");
            start=std::chrono::steady_clock::now();model.reset();model.reset();
            const auto repeatedMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
            std::cout<<nlohmann::json{{"directoryResetCycle",cycle},{"unretainedSnapshotResetMs",repeatedMs}}.dump()<<std::endl;
            if(repeatedMs>100)throw std::runtime_error("Repeated directory teardown must retain the 100ms budget");
            if(model.statistics().at("snapshots")!=0||!model.tree().empty())throw std::runtime_error("Reset must immediately clear visible content");
        }
        if(snapshot->assets.size()!=10000)throw std::runtime_error("Retired immutable snapshots must remain valid for their consumers");
        std::size_t peakBytes=0;
#ifdef _WIN32
        PROCESS_MEMORY_COUNTERS memory{};memory.cb=sizeof(memory);if(GetProcessMemoryInfo(GetCurrentProcess(),&memory,sizeof(memory)))peakBytes=memory.PeakWorkingSetSize;
#endif
        std::cout<<nlohmann::json{{"assets",10000},{"warmup",300},{"samples",1800},{"firstSnapshotMs",enumeration},{"cachedP95Ms",p95},{"peakWorkingSetBytes",peakBytes},{"statistics",model.statistics()}}.dump()<<'\n';
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';status=1;}
    std::filesystem::remove_all(root);return status;
}
