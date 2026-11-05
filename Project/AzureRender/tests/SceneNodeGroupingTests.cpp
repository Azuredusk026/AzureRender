#include "scene/SceneNodeGrouping.hpp"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <random>
#include <stdexcept>

using namespace azurerender;
namespace {
std::vector<std::size_t> reference(const std::vector<std::size_t>& keys,std::size_t count) {
    std::vector<std::size_t> result;
    for(std::size_t key=0;key<count;++key)
        for(std::size_t i=0;i<keys.size();++i)if(keys[i]==key)result.push_back(i);
    return result;
}
void verify(const std::vector<std::size_t>& keys,std::size_t count) {
    if(scene::stableMeshNodeOrder(keys,count)!=reference(keys,count))
        throw std::runtime_error("Stable grouping changed source ownership or resource order");
}
}
int main()try {
    verify({},0);verify({},7);verify({0,1,99},0);
    verify({2,0,2,1,9,0,1,2,9},3);verify({9,9},3);
    std::mt19937 random(0x695ACA);
    std::vector<std::size_t> keys(1679);
    for(unsigned round=0;round<32;++round){
        for(auto& key:keys)key=random()%400;verify(keys,374);
        std::reverse(keys.begin(),keys.end());verify(keys,374);
    }
    keys.resize(65536);for(auto& key:keys)key=random()%1100;verify(keys,1024);
    auto measure=[&](auto action){
        const auto start=std::chrono::steady_clock::now();std::size_t checksum=0;
        for(unsigned i=0;i<8;++i){const auto values=action();checksum+=values.size()+values.at(i);}
        return std::make_pair(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count(),checksum);
    };
    const auto old=measure([&]{return reference(keys,1024);});
    const auto candidate=measure([&]{return scene::stableMeshNodeOrder(keys,1024);});
    if(old.second!=candidate.second)throw std::runtime_error("Measured work checksum differs");
    std::cout<<"Stable grouping: deterministic ordering, invalid keys, reverse and randomized scenes passed\n"
             <<"Diagnostic 65536 nodes/1024 meshes,8 repetitions: referenceMs="<<old.first<<" candidateMs="<<candidate.first<<" checksum="<<old.second<<'\n';
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
