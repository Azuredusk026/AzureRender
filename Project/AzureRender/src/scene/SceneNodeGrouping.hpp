#pragma once

#include <cstddef>
#include <vector>

namespace azurerender::scene {

// Mesh order defines contiguous draw spans. Stable scatter retains source-node
// order within each mesh, including identities used by shadows and motion.
// Keys outside the loaded mesh range represent nodes omitted from drawing.
[[nodiscard]] inline std::vector<std::size_t> stableMeshNodeOrder(
    const std::vector<std::size_t>& meshKeys,std::size_t meshCount) {
    if(meshKeys.empty()||meshCount==0)return {};
    std::vector<std::size_t> offsets(meshCount,0);
    for(const auto key:meshKeys)if(key<meshCount)++offsets[key];
    std::size_t total=0;
    for(auto& offset:offsets){const auto count=offset;offset=total;total+=count;}
    std::vector<std::size_t> result(total);
    for(std::size_t i=0;i<meshKeys.size();++i)
        if(meshKeys[i]<meshCount)result[offsets[meshKeys[i]]++]=i;
    return result;
}

} // namespace azurerender::scene
