#pragma once

#include "scene/SceneDescription.hpp"
#include <cstring>
#include <new>

namespace azurerender::scene {

// Exact input cache for one consumer. Result references expire at the next
// resolve or clear; published frame instances copy their matrices by value.
class SceneTransformCache final {
public:
    struct Statistics {
        std::size_t nodes=0,localEvaluations=0,worldEvaluations=0,reusedWorld=0;
        bool topologyRebuilt=false,legacyFallback=false;
    };
    explicit SceneTransformCache(std::size_t nodeLimit=65536):nodeLimit_(nodeLimit<=65536?nodeLimit:65536){}

    const std::vector<internal::Matrix4>& resolve(const SceneDescription& scene,std::size_t* cycleCount=nullptr) {
        stats_={};stats_.nodes=scene.nodes.size();if(cycleCount)*cycleCount=0;
        if(scene.nodes.size()>nodeLimit_)return complete(scene,cycleCount);
        try {
            const auto count=scene.nodes.size();
            bool topology=!ready_||entries_.size()!=count;
            if(!topology)for(std::size_t index=0;index<count;++index)
                if(entries_[index].id!=scene.nodes[index].id||entries_[index].parentId!=scene.nodes[index].parentId){topology=true;break;}
            if(topology) {
                ready_=false;
                std::vector<Entry> entries(count);
                std::unordered_map<std::string,std::size_t> indices;indices.reserve(count);
                for(std::size_t index=0;index<count;++index) {
                    entries[index].id=scene.nodes[index].id;entries[index].parentId=scene.nodes[index].parentId;
                    if(!entries[index].id.empty())indices.emplace(entries[index].id,index);
                }
                std::vector<std::size_t> parents(count,count),order;order.reserve(count);
                for(std::size_t index=0;index<count;++index) {
                    const auto found=indices.find(entries[index].parentId);
                    if(!entries[index].parentId.empty()&&found!=indices.end()&&found->second!=index)parents[index]=found->second;
                }
                std::vector<std::uint8_t> state(count,0);bool cyclic=false;
                const auto visit=[&](std::size_t index,auto&& self)->void {
                    if(state[index]==2)return;
                    if(state[index]==1){cyclic=true;return;}
                    state[index]=1;
                    if(parents[index]!=count)self(parents[index],self);
                    state[index]=2;order.push_back(index);
                };
                for(std::size_t index=0;index<count;++index)visit(index,visit);
                if(cyclic)return complete(scene,cycleCount);
                entries_=std::move(entries);parents_=std::move(parents);order_=std::move(order);
                locals_.resize(count);world_.resize(count);localChanged_.resize(count);worldChanged_.resize(count);
            }
            for(std::size_t index=0;index<count;++index) {
                const auto& node=scene.nodes[index];const auto& entry=entries_[index];
                const bool changed=topology||!same(entry.translation,node.translation)||!same(entry.rotation,node.rotation)||!same(entry.scale,node.scale);
                localChanged_[index]=changed?1:0;
                if(changed&&(!finite(node.translation)||!finite(node.rotation)||!finite(node.scale)))return complete(scene,cycleCount);
            }
            stats_.topologyRebuilt=topology;
            for(const auto index:order_) {
                const auto& node=scene.nodes[index];const auto parent=parents_[index];
                if(localChanged_[index]) {
                    locals_[index]=composeTrs(node.translation,node.rotation,node.scale);
                    if(!finite(locals_[index]))return complete(scene,cycleCount);
                    auto& entry=entries_[index];entry.translation=node.translation;entry.rotation=node.rotation;entry.scale=node.scale;
                    ++stats_.localEvaluations;
                }
                const bool changed=topology||localChanged_[index]||(parent!=count&&worldChanged_[parent]);
                worldChanged_[index]=changed?1:0;
                if(changed) {
                    world_[index]=parent==count?locals_[index]:internal::multiply(world_[parent],locals_[index]);
                    if(!finite(world_[index]))return complete(scene,cycleCount);
                    ++stats_.worldEvaluations;
                } else ++stats_.reusedWorld;
            }
            ready_=true;return world_;
        } catch(const std::bad_alloc&) {return complete(scene,cycleCount);}
    }
    void clear() noexcept {
        releaseMetadata();std::vector<internal::Matrix4>{}.swap(world_);stats_={};
    }
    [[nodiscard]] const Statistics& statistics() const noexcept{return stats_;}
private:
    struct Entry {
        std::string id,parentId;
        std::array<float,3> translation{},rotation{},scale{};
    };
    static bool same(const std::array<float,3>& a,const std::array<float,3>& b) noexcept {
        return std::memcmp(a.data(),b.data(),a.size()*sizeof(float))==0;
    }
    template<std::size_t N> static bool finite(const std::array<float,N>& values) noexcept {
        for(const float value:values)if(!std::isfinite(value))return false;
        return true;
    }
    void releaseMetadata() noexcept {
        std::vector<Entry>{}.swap(entries_);std::vector<std::size_t>{}.swap(parents_);std::vector<std::size_t>{}.swap(order_);
        std::vector<internal::Matrix4>{}.swap(locals_);
        std::vector<std::uint8_t>{}.swap(localChanged_);std::vector<std::uint8_t>{}.swap(worldChanged_);ready_=false;
    }
    const std::vector<internal::Matrix4>& complete(const SceneDescription& scene,std::size_t* cycleCount) {
        releaseMetadata();std::vector<internal::Matrix4>{}.swap(world_);
        world_=resolveNodeWorldTransforms(scene,cycleCount);
        stats_={scene.nodes.size(),scene.nodes.size(),scene.nodes.size(),0,false,true};
        return world_;
    }
    std::size_t nodeLimit_;
    bool ready_=false;
    std::vector<Entry> entries_;
    std::vector<std::size_t> parents_,order_;
    std::vector<internal::Matrix4> locals_,world_;
    std::vector<std::uint8_t> localChanged_,worldChanged_;
    Statistics stats_;
};
}
