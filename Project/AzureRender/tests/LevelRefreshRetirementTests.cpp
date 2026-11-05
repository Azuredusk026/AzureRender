#include "runtime/LevelSession.hpp"
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <new>
#include <thread>
#include <algorithm>
#include <limits>

namespace probe {
constexpr std::size_t large=512*1024;
std::atomic<std::size_t> liveLarge{0},mainPollFree{0};
thread_local bool inPoll=false;
struct alignas(std::max_align_t) Header {std::size_t bytes;};
void* allocate(std::size_t bytes){
    const auto size=std::max<std::size_t>(bytes,1);
    if(size>std::numeric_limits<std::size_t>::max()-sizeof(Header))throw std::bad_alloc();
    auto* value=static_cast<Header*>(std::malloc(sizeof(Header)+size));
    if(!value)throw std::bad_alloc();value->bytes=size;
    if(size>=large)liveLarge.fetch_add(size);return value+1;
}
void release(void* value)noexcept{
    if(!value)return;auto* header=static_cast<Header*>(value)-1;
    if(header->bytes>=large){liveLarge.fetch_sub(header->bytes);if(inPoll)mainPollFree.fetch_add(header->bytes);}
    std::free(header);
}
}
void* operator new(std::size_t size){return probe::allocate(size);}
void* operator new[](std::size_t size){return probe::allocate(size);}
void operator delete(void* p)noexcept{probe::release(p);}
void operator delete[](void* p)noexcept{probe::release(p);}
void operator delete(void* p,std::size_t)noexcept{probe::release(p);}
void operator delete[](void* p,std::size_t)noexcept{probe::release(p);}
using namespace azurerender;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
}
int main()try{
    auto temp=std::filesystem::absolute(std::filesystem::temp_directory_path()).lexically_normal();
    while(temp.filename().empty()&&temp!=temp.root_path())temp=temp.parent_path();
    const auto root=temp/("azure-level-retire-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    require(root.is_absolute()&&root.parent_path()==temp&&std::filesystem::weakly_canonical(root).parent_path()==std::filesystem::weakly_canonical(temp)&&root.filename().string().rfind("azure-level-retire-",0)==0&&!std::filesystem::exists(root),"Fixture root must remain in its fresh named temporary directory");
    auto cleanup=std::unique_ptr<void,std::function<void(void*)>>(&probe::inPoll,[root](void*){std::filesystem::remove_all(root);});
    const auto before=probe::liveLarge.load();
    {
        Project::create(root,"Refresh retirement");
        auto project=Project::load(root/"project.azureproject");
        {
            auto document=nlohmann::json::parse(R"({"schemaVersion":1,"id":"retirement","sceneType":"sample","resources":[],"prefabs":[],"nodes":[{"id":"node","components":{}}]})");
            document["diagnosticPadding"]=std::string(1024*1024,'x');
            std::ofstream stream(root/"assets/retirement.azurelevel");stream<<document;stream.close();require(static_cast<bool>(stream),"Fixture level write failed");
        }
        project.startupScene="assets:/retirement.azurelevel";
        RuntimeLifecycle runtime;runtime.start();LevelSession session(project,runtime);
        const auto entities=runtime.world().entityCount();std::size_t firstRetained=0;
        for(unsigned cycle=0;cycle<3;++cycle){
            const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(8);bool started=false;
            while(std::chrono::steady_clock::now()<deadline){
                probe::inPoll=true;bool changed=false;
                try{changed=session.poll();}catch(...){probe::inPoll=false;throw;}
                probe::inPoll=false;
                require(!changed,"Unchanged automatic refresh must not commit a scene");
                if(session.loading())started=true;
                if(started&&!session.loading())break;
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
            }
            require(started&&!session.loading()&&session.lastError().empty(),"Automatic refresh must settle within fixture budget");
            require(session.revision()==1&&runtime.world().entityCount()==entities,"Unchanged refresh must preserve scene revision and entities");
            const auto retained=probe::liveLarge.load();if(cycle==0)firstRetained=retained;
            require(retained<=firstRetained+probe::large,"Retired refresh data must remain bounded across repeated work");
            std::cout<<"cycle="<<cycle<<" mainPollLargeFreeBytes="<<probe::mainPollFree.load()<<" retainedLargeBytes="<<retained<<std::endl;
        }
        require(probe::mainPollFree.load()==0,"Unchanged refresh must retire large candidate documents outside main-thread poll");
        runtime.stop();
    }
    require(probe::liveLarge.load()==before,"Session shutdown must release every large owned allocation");
    std::cout<<"Automatic refresh retirement: main-thread release, bounded retention, unchanged revisions and shutdown passed\n";return 0;
}catch(const std::exception& e){probe::inPoll=false;std::cerr<<e.what()<<'\n';return 1;}
