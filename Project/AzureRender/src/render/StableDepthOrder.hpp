#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <vector>

namespace azurerender {
struct DepthOrderWorkspace {
    std::vector<std::uint32_t> scratch,keys;
    std::uint64_t calls=0,keyEvaluations=0,scatterPasses=0,skippedPasses=0;
};
// Stable descending order of indexed binary32 depths. Equal depths, including
// signed zero, retain their input order. Reusable workspace is caller-owned.
inline void stableDepthOrder(std::vector<std::uint32_t>& order,const std::vector<float>& depths,
                             DepthOrderWorkspace& workspace) {
    static_assert(sizeof(float)==sizeof(std::uint32_t)&&std::numeric_limits<float>::is_iec559,
        "Depth ordering requires IEEE binary32");
    if(&order==&workspace.scratch||&order==&workspace.keys)
        throw std::invalid_argument("Depth input must not alias workspace storage");
    bool finite=true,sorted=true,dense=true;
    ++workspace.calls;
    for(std::size_t i=0;i<order.size();++i) {
        if(order[i]>=depths.size())throw std::out_of_range("Depth index is outside its input");
        finite=finite&&std::isfinite(depths[order[i]]);
        dense=dense&&order[i]==i;
        if(i&&depths[order[i-1]]<depths[order[i]])sorted=false;
    }
    if(!finite||order.size()<256) {
        std::stable_sort(order.begin(),order.end(),[&](auto x,auto y){return depths[x]>depths[y];});
        return;
    }
    if(sorted)return;
    const auto key=[&](std::uint32_t index) {
        const float value=depths[index];
        std::uint32_t bits=0;
        if(value!=0)std::memcpy(&bits,&value,sizeof(bits));
        return (bits&0x80000000U)?bits:(bits^0x7fffffffU);
    };
    workspace.scratch.resize(order.size());
    if(dense)workspace.keys.resize(order.size());
    std::array<std::array<std::size_t,256>,4> histograms{};
    for(auto index:order){
        const auto value=key(index);
        if(dense)workspace.keys[index]=value;
        for(unsigned pass=0;pass<4;++pass)++histograms[pass][(value>>(pass*8))&255U];
    }
    workspace.keyEvaluations+=order.size();
    for(unsigned pass=0;pass<4;++pass) {
        auto offsets=histograms[pass];
        if(std::find(offsets.begin(),offsets.end(),order.size())!=offsets.end()){
            ++workspace.skippedPasses;continue;
        }
        const unsigned shift=pass*8;
        std::size_t begin=0;
        for(auto& count:offsets) {const auto size=count;count=begin;begin+=size;}
        for(auto index:order){
            const auto value=dense?workspace.keys[index]:key(index);
            workspace.scratch[offsets[(value>>shift)&255U]++]=index;
        }
        if(!dense)workspace.keyEvaluations+=order.size();
        order.swap(workspace.scratch);++workspace.scatterPasses;
    }
}
inline void stableDepthOrder(std::vector<std::uint32_t>& order,const std::vector<float>& depths) {
    DepthOrderWorkspace workspace;
    stableDepthOrder(order,depths,workspace);
}
} // namespace azurerender
