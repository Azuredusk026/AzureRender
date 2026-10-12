#include "runtime/AudioNotificationQueue.hpp"
#include <atomic>
#include <future>
#include <iostream>
using namespace azurerender;
int main(){try{
    std::promise<void> entered,release;auto ready=release.get_future().share();std::atomic<unsigned> calls{0};AudioNotificationQueue queue;
    if(!queue.post([&]{entered.set_value();ready.wait();++calls;}))throw std::runtime_error("Initial callback must be accepted");
    if(entered.get_future().wait_for(std::chrono::seconds(2))!=std::future_status::ready){release.set_value();throw std::runtime_error("Callback worker must start");}
    const auto start=std::chrono::steady_clock::now();const bool posted=queue.post([&]{++calls;});
    release.set_value();queue.close();
    if(!posted||std::chrono::steady_clock::now()-start>std::chrono::seconds(1)||calls<1||queue.post([]{}))throw std::runtime_error("Notification submission must be asynchronous and shutdown must reject new work");
    queue.close();std::cout<<"Asynchronous device notifications and idempotent shutdown passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
