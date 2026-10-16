#include "render/SkinningBatch.hpp"
#include "render/MorphGpuData.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace azurerender;
void check(bool condition, const char* reason) { if (!condition) throw std::runtime_error(reason); }
int main() { try {
    LoadedAsset asset;asset.vertices.resize(1);asset.nodes.resize(1);
    MorphTargetRange range;range.node=0;range.vertexCount=1;range.targets.resize(3);
    range.targets[2].positions={{{2,3,4}}};range.targets[2].normals={{{1,0,0}}};range.targets[2].tangents={{{0,1,0}}};asset.morphRanges={range};
    std::vector<std::uint32_t> packed{0,0,0};const auto gpu=appendMorphGpuMesh(packed,asset);
    check(gpu.tableBase==3&&gpu.weightCount==3&&packed[5]==3,"Packed table retains third target");
    AssetPose pose;pose.morphWeights={{{0,0,.75F}}};const auto weights=flattenMorphGpuWeights(asset,pose);
    check(weights==std::vector<float>({0,0,.75F}),"Flatten weights use node pose");
    for(unsigned invalid=0;invalid<4;++invalid){
        auto broken=asset;
        if(invalid==0)broken.morphRanges.push_back(range);
        if(invalid==1)broken.morphRanges[0].targets[0].positions.resize(2);
        if(invalid==2)broken.morphRanges[0].targets[2].positions[0][0]=std::numeric_limits<float>::infinity();
        if(invalid==3)broken.morphRanges[0].node=1;
        const auto original=packed;bool rejected=false;
        try{appendMorphGpuMesh(packed,broken);}catch(const std::invalid_argument&){rejected=true;}
        check(rejected&&packed==original,"Malformed morph meshes reject transactionally");
    }
    for(unsigned invalid=0;invalid<2;++invalid){
        auto broken=pose;if(invalid==0)broken.morphWeights[0].pop_back();else broken.morphWeights[0][0]=std::numeric_limits<float>::quiet_NaN();
        bool rejected=false;try{flattenMorphGpuWeights(asset,broken);}catch(const std::invalid_argument&){rejected=true;}
        check(rejected,"Invalid per-instance morph weights reject");
    }
    SkinningBatch varying{1,0,{0,0},0,1,1};varying.morphTableBase=gpu.tableBase;varying.morphWeightStride=3;
    auto adjacent=varying;adjacent.jointBase=1;adjacent.outputBase=1;adjacent.morphWeightBase=3;adjacent.morphWeights={1,1};
    check(appendSkinningSlice(varying,adjacent),"Independent contiguous full weights batch despite different legacy values");
    SkinningBatch first{337, 0, {0.2F,0.3F}, 0, 4, 1};
    const SkinningBatch next{337, 4, {0.2F,0.3F}, 337, 4, 1};
    check(appendSkinningSlice(first,next), "Independent contiguous joints must batch");
    check(first.instanceCount==2 && first.jointBase==0 && first.outputBase==0,
        "Batch preserves first slice and records both independent slices");
    for (unsigned change=0; change<5; ++change) {
        SkinningBatch batch{337, 0, {0.2F,0.3F}, 0, 4, 1};
        auto incompatible=next;
        if(change==0) incompatible.outputBase=674;
        if(change==1) incompatible.jointBase=8;
        if(change==2) incompatible.jointStride=5;
        if(change==3) incompatible.vertexCount=338;
        if(change==4) incompatible.morphWeights[0]=0.5F;
        check(!appendSkinningSlice(batch,incompatible) && batch.instanceCount==1,
            "Gaps, mesh shape and morph differences must retain separate work");
    }
    SkinningBatch full{1,0,{0,0},0,1,65535};
    check(!appendSkinningSlice(full,{1,65535,{0,0},65535,1,1}), "Vulkan portable Y limit");
    const auto maximum=std::numeric_limits<std::uint32_t>::max();
    SkinningBatch overflow{10,maximum-2,{0,0},maximum-2,10,1};
    check(!appendSkinningSlice(overflow,{10,7,{0,0},7,10,1}), "Wrapped offsets must not merge");
    SkinningBatch empty{0,0,{0,0},0,0,1};
    check(!appendSkinningSlice(empty,empty), "Empty geometry must not form a batch");
    std::cout << "Skinning slices, independent joints, morph boundaries and dispatch limits passed\n";
} catch(const std::exception& error) { std::cerr << error.what() << '\n'; return 1; } }
