#include "render/PrimitiveOpacity.hpp"
#include "render/OutlineMaskLayout.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
using namespace azurerender;
std::vector<PrimitiveOpacityRun> runs(const std::vector<std::vector<float>>& values,
    std::uint32_t node, std::uint32_t first, std::uint32_t count) {
    std::vector<PrimitiveOpacityRun> result;
    forEachPrimitiveOpacityRun(values,node,first,count,[&](const auto& value) { result.push_back(value); });
    return result;
}
void require(bool condition,const char* message) { if(!condition)throw std::runtime_error(message); }
}

int main() {
    try {
        const auto defaults=runs({},std::numeric_limits<std::uint32_t>::max(),4,101);
        require(defaults.size()==1 && defaults[0].firstInstance==4 && defaults[0].instanceCount==101 && defaults[0].opacity==1,
            "Default opacity must retain one original instance batch");
        const std::vector<std::vector<float>> values{{1,0.2F},{1,0.2F},{0.5F,1},{0.5F,1},{0,0.8F},{1,0.2F}};
        const auto changed=runs(values,0,0,6);
        require(changed.size()==4 && changed[0].firstInstance==0 && changed[0].instanceCount==2 && changed[0].opacity==1
            && changed[1].firstInstance==2 && changed[1].instanceCount==2 && changed[1].opacity==0.5F
            && changed[2].firstInstance==4 && changed[2].instanceCount==1 && changed[2].opacity==0
            && changed[3].firstInstance==5 && changed[3].instanceCount==1 && changed[3].opacity==1,
            "Independent instance ages must split only contiguous equal opacity runs");
        const auto otherNode=runs(values,1,0,6);
        require(otherNode.size()==4 && otherNode[0].opacity==0.2F && otherNode[1].opacity==1 && otherNode[2].opacity==0.8F,
            "Primitive opacity must come from its own source node");
        const auto subrange=runs(values,0,1,3);
        require(subrange.size()==2 && subrange[0].firstInstance==1 && subrange[0].instanceCount==1
            && subrange[1].firstInstance==2 && subrange[1].instanceCount==2,
            "Split ranges must retain source instance indices for indirect drawing");
        require(runs(values,0,6,0).empty(),"An empty visibility range must not emit a draw");
        unsigned rejected=0;
        for (const auto invalid : {-0.1F,1.1F,std::numeric_limits<float>::quiet_NaN()}) {
            try { (void)runs({{invalid}},0,0,1); } catch(const std::invalid_argument&) { ++rejected; }
        }
        require(rejected==3,"Invalid snapshot opacity must fail before recording");
        bool rangeRejected=false,nodeRejected=false;
        try { (void)runs(values,0,5,2); } catch(const std::out_of_range&) { rangeRejected=true; }
        try { (void)runs(values,2,0,1); } catch(const std::out_of_range&) { nodeRejected=true; }
        require(rangeRejected&&nodeRejected,"Invalid source mapping must not silently use a default opacity");
        const auto absent=makeOutlineMaskLayout(100,{},2560,1440,16384);
        require(absent.width==0&&absent.height==0,"Fully opaque scenes must allocate no outline mask image");
        const auto layout=makeOutlineMaskLayout(7,{1,5},256,192,4096);
        require(layout.columns==2&&layout.width==512&&layout.height==192
            &&layout.instanceTiles[1]==0&&layout.instanceTiles[5]==1
            &&layout.instanceTiles[0]==std::numeric_limits<std::uint32_t>::max(),
            "Overlapping fading instances must own separate full-view mask tiles");
        unsigned invalidLayouts=0;
        try{(void)makeOutlineMaskLayout(7,{1,1},256,192,4096);}catch(const std::invalid_argument&){++invalidLayouts;}
        try{(void)makeOutlineMaskLayout(7,{7},256,192,4096);}catch(const std::out_of_range&){++invalidLayouts;}
        try{(void)makeOutlineMaskLayout(7,{1,5},256,192,256);}catch(const std::length_error&){++invalidLayouts;}
        try{(void)makeOutlineMaskLayout(7,{1,5},256,192,4096,256*192);}catch(const std::length_error&){++invalidLayouts;}
        require(invalidLayouts==4,"Mask source ownership, dimensions and memory budget must be checked");
        std::cout << "Primitive opacity preserves default batches and independent node coverage\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr << error.what() << '\n';return 1;
    }
}
