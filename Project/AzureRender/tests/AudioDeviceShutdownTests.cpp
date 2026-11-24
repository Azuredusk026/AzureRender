#include "runtime/AudioRuntime.hpp"
#include <chrono>
#include <iostream>
using namespace azurerender;
int main(){try{
    unsigned devices=0;const auto start=std::chrono::steady_clock::now();
    for(unsigned i=0;i<8;++i){AudioRuntime runtime(true);if(runtime.hasDevice())++devices;runtime.pauseAll(true);runtime.pauseAll(false);}
    if(!devices){std::cout<<"No live audio device; hardware shutdown remains unverified\n";return 77;}
    if(std::chrono::steady_clock::now()-start>std::chrono::seconds(15))throw std::runtime_error("Audio device shutdown exceeded the short-cycle budget");
    std::cout<<"Eight live-device initialization and teardown cycles passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
