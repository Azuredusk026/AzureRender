#include "runtime/RuntimeRenderSnapshotCache.hpp"
#include <limits>
#include <stdexcept>
#include <unordered_map>

namespace azurerender {
namespace {constexpr auto noParent=std::numeric_limits<std::size_t>::max();}
void RuntimeRenderSnapshotCache::clear() noexcept {
    source_.reset();base_={};parents_.clear();lightNodes_.clear();matrices_.clear();states_.clear();frames_.clear();
}
bool RuntimeRenderSnapshotCache::sameLayout(const SceneDocument& document) const {
    if(document.nodes.size()!=base_.nodes.size()||document.lights.size()!=base_.lights.size())return false;
    for(std::size_t i=0;i<document.nodes.size();++i)
        if(document.nodes[i].id!=base_.nodes[i].id||document.nodes[i].parentId!=base_.nodes[i].parentId)return false;
    for(std::size_t i=0;i<document.lights.size();++i)
        if(document.lights[i].id!=base_.lights[i].id||document.lights[i].nodeId!=base_.lights[i].nodeId)return false;
    return true;
}
std::shared_ptr<const scene::SceneDescription> RuntimeRenderSnapshotCache::prepare(
    const SceneDocument& document,std::shared_ptr<const void> source,const Prepare& prepareStatic){
    if(!source)throw std::invalid_argument("Runtime render snapshot requires an owned source identity");
    if(source_!=source||!sameLayout(document)){
        auto base=prepareStatic(document);
        if(base.nodes.size()!=document.nodes.size()||base.lights.size()!=document.lights.size())
            throw std::invalid_argument("Runtime render preparation must preserve node and light order");
        for(std::size_t i=0;i<document.nodes.size();++i)
            if(base.nodes[i].id!=document.nodes[i].id||base.nodes[i].parentId!=document.nodes[i].parentId)
                throw std::invalid_argument("Runtime render preparation reordered scene nodes");
        for(std::size_t i=0;i<document.lights.size();++i)
            if(base.lights[i].id!=document.lights[i].id||base.lights[i].nodeId!=document.lights[i].nodeId)
                throw std::invalid_argument("Runtime render preparation reordered scene lights");
        std::unordered_map<std::string,std::size_t> indices;indices.reserve(document.nodes.size());
        for(std::size_t i=0;i<document.nodes.size();++i)indices.emplace(document.nodes[i].id,i);
        std::vector<std::size_t> parents(document.nodes.size(),noParent),lightNodes;
        for(std::size_t i=0;i<document.nodes.size();++i){const auto parent=indices.find(document.nodes[i].parentId);
            if(!document.nodes[i].parentId.empty()&&parent!=indices.end())parents[i]=parent->second;}
        lightNodes.reserve(document.lights.size());
        for(const auto& light:document.lights){const auto found=indices.find(light.nodeId);
            if(found==indices.end())throw std::runtime_error("Scene light references missing node: "+light.nodeId);
            lightNodes.push_back(found->second);}
        base_=std::move(base);parents_=std::move(parents);lightNodes_=std::move(lightNodes);
        source_=std::move(source);frames_.clear();
    }
    std::shared_ptr<scene::SceneDescription> frame;
    for(const auto& candidate:frames_)if(candidate.use_count()==1){frame=candidate;break;}
    if(!frame){frame=std::make_shared<scene::SceneDescription>(base_);if(frames_.size()<3)frames_.push_back(frame);}
    for(std::size_t i=0;i<document.nodes.size();++i){const auto& node=document.nodes[i];auto& result=frame->nodes[i];
        if(node.objectShadow)node.objectShadow->validate();
        result.translation=node.translation;result.rotation=node.rotation;result.scale=node.scale;
        result.visible=node.visible;result.objectShadow=node.objectShadow;
    }
    matrices_.resize(document.nodes.size());states_.assign(document.nodes.size(),0);
    const auto resolve=[&](std::size_t index,auto&& self)->const internal::Matrix4&{
        if(states_[index]==2)return matrices_[index];
        if(states_[index]==1)throw std::runtime_error("Scene transform hierarchy contains a cycle");
        states_[index]=1;const auto& node=document.nodes[index];
        const auto local=scene::composeTrs(node.translation,node.rotation,node.scale);
        matrices_[index]=parents_[index]==noParent?local:internal::multiply(self(parents_[index],self),local);
        states_[index]=2;return matrices_[index];
    };
    for(std::size_t i=0;i<document.lights.size();++i){const auto& light=document.lights[i];auto& result=frame->lights[i];
        const auto& world=resolve(lightNodes_[i],resolve);
        result.position=internal::transformPosition(world,{0.F,0.F,0.F});
        result.color=light.color;result.intensity=light.intensity;result.radius=light.radius;result.enabled=light.enabled;
        result.shape=light.shape;result.shape.validate();const auto local=result.shape.direction;float lengthSquared=0;
        for(unsigned axis=0;axis<3;++axis){result.shape.direction[axis]=world[axis]*local[0]+world[axis+4]*local[1]+world[axis+8]*local[2];lengthSquared+=result.shape.direction[axis]*result.shape.direction[axis];}
        if(!(lengthSquared>1e-12F))throw std::runtime_error("Degenerate light node transform");
        for(auto& value:result.shape.direction)value/=std::sqrt(lengthSquared);
    }
    return frame;
}
}
