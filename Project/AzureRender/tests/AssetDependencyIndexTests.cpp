#include "runtime/AssetDatabase.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <new>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#endif

namespace allocations {
std::atomic<std::size_t> live{0},peak{0};
struct Header {void* allocation;std::size_t size;};
void* acquire(std::size_t size,std::size_t alignment=alignof(std::max_align_t)) {
    size=std::max<std::size_t>(size,1);
    alignment=std::max(alignment,alignof(std::max_align_t));
    if(size>std::numeric_limits<std::size_t>::max()-alignment-sizeof(Header))throw std::bad_alloc();
    auto* raw=std::malloc(size+alignment+sizeof(Header));
    if(!raw)throw std::bad_alloc();
    auto address=(reinterpret_cast<std::uintptr_t>(raw)+sizeof(Header)+alignment-1)&~(alignment-1);
    auto* header=reinterpret_cast<Header*>(address)-1;*header={raw,size};
    const auto current=live.fetch_add(size,std::memory_order_relaxed)+size;
    auto previous=peak.load(std::memory_order_relaxed);
    while(previous<current&&!peak.compare_exchange_weak(previous,current,std::memory_order_relaxed)){}
    return reinterpret_cast<void*>(address);
}
void release(void* pointer) noexcept {
    if(!pointer)return;
    const auto* header=static_cast<Header*>(pointer)-1;
    live.fetch_sub(header->size,std::memory_order_relaxed);std::free(header->allocation);
}
}
void* operator new(std::size_t size){return allocations::acquire(size);}
void* operator new[](std::size_t size){return allocations::acquire(size);}
void operator delete(void* pointer) noexcept {allocations::release(pointer);}
void operator delete[](void* pointer) noexcept {allocations::release(pointer);}
void operator delete(void* pointer,std::size_t) noexcept {allocations::release(pointer);}
void operator delete[](void* pointer,std::size_t) noexcept {allocations::release(pointer);}
void* operator new(std::size_t size,std::align_val_t alignment){return allocations::acquire(size,static_cast<std::size_t>(alignment));}
void* operator new[](std::size_t size,std::align_val_t alignment){return allocations::acquire(size,static_cast<std::size_t>(alignment));}
void operator delete(void* pointer,std::align_val_t) noexcept {allocations::release(pointer);}
void operator delete[](void* pointer,std::align_val_t) noexcept {allocations::release(pointer);}
void operator delete(void* pointer,std::size_t,std::align_val_t) noexcept {allocations::release(pointer);}
void operator delete[](void* pointer,std::size_t,std::align_val_t) noexcept {allocations::release(pointer);}
void* operator new(std::size_t size,const std::nothrow_t&) noexcept {try{return allocations::acquire(size);}catch(...){return nullptr;}}
void* operator new[](std::size_t size,const std::nothrow_t&) noexcept {try{return allocations::acquire(size);}catch(...){return nullptr;}}
void operator delete(void* pointer,const std::nothrow_t&) noexcept {allocations::release(pointer);}
void operator delete[](void* pointer,const std::nothrow_t&) noexcept {allocations::release(pointer);}
void* operator new(std::size_t size,std::align_val_t alignment,const std::nothrow_t&) noexcept {try{return allocations::acquire(size,static_cast<std::size_t>(alignment));}catch(...){return nullptr;}}
void* operator new[](std::size_t size,std::align_val_t alignment,const std::nothrow_t&) noexcept {try{return allocations::acquire(size,static_cast<std::size_t>(alignment));}catch(...){return nullptr;}}
void operator delete(void* pointer,std::align_val_t,const std::nothrow_t&) noexcept {allocations::release(pointer);}
void operator delete[](void* pointer,std::align_val_t,const std::nothrow_t&) noexcept {allocations::release(pointer);}

namespace {
using namespace azurerender;
using Json=nlohmann::json;
namespace fs=std::filesystem;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void write(const fs::path& path,const std::string& text){std::ofstream file(path,std::ios::binary);file<<text;require(bool(file),"Fixture write failed");}
void largeDocument(const fs::path& path,const Json& document) {
    auto prefix=document.dump();require(document.is_object(),"Large fixture root must be an object");prefix.pop_back();
    std::ofstream file(path,std::ios::binary);file<<prefix<<",\"authoringPayload\":\"";
    const std::string chunk(4096,'x');
    for(unsigned i=0;i<1024;++i)file<<chunk;
    file<<"\"}";require(bool(file),"Large fixture write failed");
}
Json records(const AssetDatabase& database) {
    Json result=Json::object();
    for(const auto& item:database.records()) {
        const auto& record=item.second;
        result[record.virtualPath]={{"id",record.id},{"contentHash",record.contentHash},
            {"fingerprint",record.fingerprint},{"dependencies",record.dependencies}};
    }
    return result;
}
void snapshot(const fs::path& project,const fs::path& report) {
    const auto initial=allocations::live.load();allocations::peak=initial;
    const auto start=std::chrono::steady_clock::now();
    AssetDatabase database(Project::load(project));database.refresh();
    const auto elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
    const auto retained=allocations::live.load()-initial,peak=allocations::peak.load()-initial;
    Json result={{"schemaVersion",1},{"coldSeconds",elapsed},{"retainedAllocationBytes",retained},
        {"peakTrackedAllocationBytes",peak},{"records",records(database)}};
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS_EX memory{};memory.cb=sizeof(memory);
    require(GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory),sizeof(memory)),"Process memory observation failed");
    result["peakWorkingSetBytes"]=memory.PeakWorkingSetSize;result["privateBytes"]=memory.PrivateUsage;
#endif
    const auto warmStart=std::chrono::steady_clock::now();database.refresh();
    result["warmSeconds"]=std::chrono::duration<double>(std::chrono::steady_clock::now()-warmStart).count();
    result["warmFilesReused"]=database.refreshStatistics().filesReused;
    require(result["records"]==records(database),"Warm refresh changed record fingerprints or dependencies");
    write(report,result.dump(2));std::cout<<"Asset scan: "<<elapsed<<" seconds, "<<retained<<" retained bytes\n";
}
void exercise(const fs::path& root,bool enforceBudget) {
    Project::create(root,"Dependency index budget");const auto assets=root/"assets";
    write(assets/"base.bin","data");
    largeDocument(assets/"material.azurematerial",Json::parse(R"({"textures":{"baseColor":{"asset":"assets:/base.bin"}}})"));
    largeDocument(assets/"model.gltf",Json::parse(R"({"asset":{"version":"2.0"},"buffers":[{"byteLength":4,"uri":"base.bin"},{"byteLength":4,"uri":"data:application/octet-stream;base64,AAAAAA=="}],"images":[{"uri":"base.bin"}],"extras":{"nested":[{"asset":"assets:/material.azurematerial"}]}})"));
    largeDocument(assets/"body.azureprefab",Json::parse(R"({"schemaVersion":1,"id":"body","resources":[{"asset":"assets:/model.gltf"}],"nodes":[]})"));
    const auto level=Json::parse(R"({"schemaVersion":1,"id":"scene","sceneType":"sample","resources":[{"asset":"assets:/body.azureprefab"}],"nodes":[],"reflectionProbe":{"texture":"assets:/base.bin","opaque":{"asset":"assets:/not-a-dependency.bin"}},"materialSlots":{"surface":"assets:/material.azurematerial"}})");
    largeDocument(assets/"scene.azurelevel",level);
    const auto project=Project::load(root/"project.azureproject");
    const auto initial=allocations::live.load();
    AssetDatabase database(project);database.refresh();
    const auto retained=allocations::live.load()-initial;
    std::cout<<"Retained index allocation: "<<retained<<" bytes for four 4 MiB documents\n";
    if(enforceBudget&&retained>2*1024*1024)throw std::runtime_error("Dependency index retained allocation exceeds 2 MiB budget: "+std::to_string(retained));
    require(database.records().size()==6,"Expected five authored assets and the default startup scene");
    const auto id=[&](const char* path){return database.idForPath(std::string("assets:/")+path);};
    const auto dependencies=[&](const char* path,std::vector<std::string> expected){
        std::sort(expected.begin(),expected.end());
        require(database.records().at(id(path)).dependencies==expected,"Authored dependency set differs from the hand-authored graph");
    };
    dependencies("material.azurematerial",{id("base.bin")});
    dependencies("model.gltf",{id("base.bin"),id("material.azurematerial")});
    dependencies("body.azureprefab",{id("model.gltf")});
    dependencies("scene.azurelevel",{id("base.bin"),id("material.azurematerial"),id("body.azureprefab")});
    dependencies("startup.azscene",{});
    const auto original=records(database);database.refresh();
    require(database.refreshStatistics().filesReused==6,"Warm refresh must reuse all unchanged source indexes");
    require(records(database)==original,"Warm refresh must preserve dependency and content fingerprints");
    auto copy=database;
    write(assets/"base.bin","changed payload");database.refresh();
    const auto updated=records(database);
    for(const auto& row:original.items()) {
        const auto same=updated.at(row.key()).at("fingerprint")==row.value().at("fingerprint");
        require(same==(row.key()=="assets:/startup.azscene"),"Transitive invalidation must preserve the independent startup scene");
    }
    require(records(copy)==original,"Refreshing a copy must preserve the shared source snapshot");
    copy.refresh();require(records(copy)==updated,"Shared source indexes must preserve dependencies on refresh");
    const auto version=database.version();
    for(const auto* invalid:{R"({"materialSlots":{"surface":3}})",R"({"reflectionProbe":{}})",R"({"nodes":[)",R"({"resources":[{"asset":"assets:/missing.bin"}]})"}) {
        write(assets/"scene.azurelevel",invalid);bool rejected=false;
        try{database.refresh();}catch(const std::exception&){rejected=true;}
        require(rejected,"Invalid dependency source must be rejected");
        require(database.version()==version&&records(database)==updated,"Failed refresh must retain the published snapshot");
    }
    largeDocument(assets/"scene.azurelevel",level);
    unsigned checks=0;bool cancelled=false;
    try{database.refresh(true,[&]{if(++checks==3)throw std::runtime_error("cancelled");});}catch(const std::exception& error){cancelled=std::string(error.what())=="cancelled";}
    require(cancelled&&database.version()==version&&records(database)==updated,"Cancelled refresh must retain the published snapshot");
    database.refresh();require(records(database)==updated,"Refresh must recover after rejection and cancellation");
    std::cout<<"Dependency budget, nested references, relative resources, warm reuse, invalidation and transactional rejection passed\n";
}
}

int main(int argc,char** argv) {
    fs::path root;
    try {
        if(argc==4&&std::string(argv[1])=="--snapshot") {snapshot(fs::u8path(argv[2]),fs::u8path(argv[3]));return 0;}
        const bool contracts=argc==3&&std::string(argv[1])=="--contracts-only";
        require(argc==2||contracts,"Expected an owned fixture parent or --snapshot project report");
        const auto parent=fs::absolute(fs::u8path(argv[contracts?2:1])).lexically_normal();
        fs::create_directories(parent);
        const auto token=std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        for(unsigned attempt=0;attempt<16&&root.empty();++attempt) {
            const auto candidate=parent/(token+"-"+std::to_string(attempt));
            if(fs::create_directory(candidate))root=candidate;
        }
        require(!root.empty()&&root.parent_path()==parent,"Cannot reserve an owned fixture directory");
        exercise(root,!contracts);fs::remove_all(root);return 0;
    }catch(const std::exception& error){if(!root.empty())fs::remove_all(root);std::cerr<<error.what()<<'\n';return 1;}
}
