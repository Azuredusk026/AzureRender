#pragma once
#include "scene/RenderBatching.hpp"
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <vector>

namespace azurerender {
// Physical identities remain stable; material draws select immutable aliases.
class MaterialOriginInstances {
public:
    struct Copy {std::uint32_t sourceIndex,node,gpuIndex;};
    MaterialOriginInstances(const std::vector<scene::SceneInstance>& source,
        const std::vector<std::vector<std::uint32_t>>& nodes) {
        if(source.size()>std::numeric_limits<std::uint32_t>::max())
            throw std::length_error("Material origin instance count exceeds GPU indexing");
        sourceCount_=count_=static_cast<std::uint32_t>(source.size());
        for(std::size_t index=0;index<source.size();++index)
            if(source[index].sourceIndex!=index||source[index].meshKey>=nodes.size())
                throw std::invalid_argument("Material origin instances require stable physical source identities");
        for(std::size_t mesh=0;mesh<nodes.size();++mesh)
            for(const auto node:std::set<std::uint32_t>(nodes[mesh].begin(),nodes[mesh].end()))
                for(const auto& instance:source)if(instance.meshKey==mesh) {
                    if(count_==std::numeric_limits<std::uint32_t>::max())
                        throw std::length_error("Material origin aliases exceed GPU indexing");
                    slots_.emplace(std::make_pair(instance.sourceIndex,node),count_);
                    copies_.push_back({instance.sourceIndex,node,count_++});
                }
    }
    std::uint32_t gpuCount()const{return count_;}
    std::uint32_t physicalCount()const{return sourceCount_;}
    const std::vector<Copy>& copies()const{return copies_;}
    std::uint32_t firstInstance(std::uint32_t node,std::uint32_t source,std::uint32_t count)const {
        if(count==0||source>=sourceCount_||count>sourceCount_-source)
            throw std::out_of_range("Material origin source run is outside physical instances");
        const auto first=slots_.find({source,node}),last=slots_.find({source+count-1,node});
        if(first==slots_.end()||last==slots_.end()||last->second-first->second!=count-1)
            throw std::invalid_argument("Material origin requires a contiguous configured instance run");
        return first->second;
    }
private:
    std::uint32_t sourceCount_=0,count_=0;
    std::vector<Copy> copies_;
    std::map<std::pair<std::uint32_t,std::uint32_t>,std::uint32_t> slots_;
};
}
