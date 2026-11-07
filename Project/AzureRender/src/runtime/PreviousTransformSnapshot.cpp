#include "PreviousTransformSnapshot.hpp"
#include "runtime/RuntimeLifecycle.hpp"
namespace azurerender {
void PreviousTransformSnapshot::clear() noexcept {
    entries_.clear();indices_.clear();complete_.clear();
    owner_=nullptr;revision_=0;fallback_=rebuilt_=false;
}
void PreviousTransformSnapshot::capture(RuntimeLifecycle& runtime) {
    const auto& world=runtime.world();
    const auto* transforms=world.findComponentArray<ecs::TransformComponent>();
    const auto count=transforms?transforms->size():0;
    rebuilt_=false;
    if(count>limit_) {
        entries_.clear();indices_.clear();complete_.clear();
        for(std::size_t i=0;i<count;++i)
            complete_[runtime.nodeId(transforms->entities()[i])]=transforms->dense()[i];
        fallback_=true;owner_=&runtime;revision_=runtime.sceneRevision();
        return;
    }
    bool aligned=!fallback_&&owner_==&runtime&&revision_==runtime.sceneRevision()
        &&entries_.size()==count;
    for(std::size_t i=0;aligned&&i<count;++i) {
        const auto entity=transforms->entities()[i];
        const auto handle=entries_[i].handle;
        aligned=handle.entity==entity&&(handle.generation?world.valid(handle):!world.valid(entity));
    }
    if(!aligned) {
        std::vector<Entry> entries;entries.reserve(count);
        std::unordered_map<std::string,std::size_t> indices;indices.reserve(count);
        for(std::size_t i=0;i<count;++i) {
            const auto entity=transforms->entities()[i];
            auto node=runtime.nodeId(entity);
            indices[std::move(node)]=i;
            entries.push_back({world.valid(entity)?world.handle(entity):ecs::EntityHandle{entity,0},
                transforms->dense()[i]});
        }
        entries_.swap(entries);indices_.swap(indices);complete_.clear();
        owner_=&runtime;revision_=runtime.sceneRevision();fallback_=false;rebuilt_=true;
    }
    for(std::size_t i=0;i<count;++i)entries_[i].value=transforms->dense()[i];
}
const ecs::TransformComponent* PreviousTransformSnapshot::find(const std::string& node) const {
    if(fallback_) {
        const auto found=complete_.find(node);
        return found==complete_.end()?nullptr:&found->second;
    }
    const auto found=indices_.find(node);
    return found==indices_.end()?nullptr:&entries_[found->second].value;
}
}
