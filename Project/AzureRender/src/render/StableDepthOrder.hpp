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
// Stable descending order of indexed binary32 depths. Equal depths, including
// signed zero, retain their input order. Each call owns its temporary storage.
inline void stableDepthOrder(std::vector<std::uint32_t>& order,const std::vector<float>& depths) {
    static_assert(sizeof(float)==sizeof(std::uint32_t)&&std::numeric_limits<float>::is_iec559,
        "Depth ordering requires IEEE binary32");
    bool finite=true,sorted=true;
    for(std::size_t i=0;i<order.size();++i) {
        if(order[i]>=depths.size())throw std::out_of_range("Depth index is outside its input");
        finite=finite&&std::isfinite(depths[order[i]]);
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
    std::vector<std::uint32_t> scratch(order.size());
    for(unsigned shift=0;shift<32;shift+=8) {
        std::array<std::size_t,256> offsets{};
        for(auto index:order)++offsets[(key(index)>>shift)&255U];
        std::size_t begin=0;
        for(auto& count:offsets) {const auto size=count;count=begin;begin+=size;}
        for(auto index:order)scratch[offsets[(key(index)>>shift)&255U]++]=index;
        order.swap(scratch);
    }
}
} // namespace azurerender
