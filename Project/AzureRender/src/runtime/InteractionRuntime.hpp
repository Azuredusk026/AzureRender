#pragma once
#include "runtime/GameComponents.hpp"
#include "runtime/PhysicsWorld.hpp"
#include <cmath>
#include <optional>
#include <limits>
namespace azurerender {
struct InteractionTarget {
    ecs::Entity actor=ecs::kInvalidEntity,target=ecs::kInvalidEntity;
    std::string actorNode,node,prompt;
    std::uint64_t revision=0;
};
class InteractionRuntime {
public:
    const std::optional<InteractionTarget>& select(RuntimeLifecycle& runtime,const PhysicsWorld& physics,ecs::Entity actor) {
        selected_.reset();
        const auto* origin=runtime.world().tryGet<ecs::TransformComponent>(actor);
        const auto actorNode=runtime.nodeId(actor);
        if(!origin||actorNode.empty())return selected_;
        float nearest=std::numeric_limits<float>::max();
        runtime.world().each<ecs::TransformComponent,game::Interactable>([&](auto target,const auto& transform,const auto& settings){
            if(!settings.enabled||target==actor)return;
            const auto node=runtime.nodeId(target);if(node.empty())return;
            if(const auto* collectible=runtime.world().tryGet<game::Collectible>(target))if(collectible->collected)return;
            if(const auto* door=runtime.world().tryGet<game::Door>(target))if(door->open)return;
            std::array<float,3> eye=origin->translation,delta{};eye[1]+=1;
            float distance=0;
            for(unsigned axis=0;axis<3;++axis){const float d=transform.translation[axis]-origin->translation[axis];distance+=d*d;
                delta[axis]=transform.translation[axis]+settings.offset[axis]-eye[axis];}
            if(distance>settings.range*settings.range)return;
            if(selected_&&(distance>nearest+1e-5F||(std::abs(distance-nearest)<=1e-5F&&node>=selected_->node)))return;
            const auto hit=physics.sphereSweep(eye,delta,.01F,actor);
            if(hit&&hit->entity!=target&&hit->fraction<.999F)return;
            nearest=distance;selected_=InteractionTarget{actor,target,actorNode,node,settings.prompt,runtime.sceneRevision()};
        });
        return selected_;
    }
    const std::optional<InteractionTarget>& target() const{return selected_;}
    void reset(){selected_.reset();}
private:
    std::optional<InteractionTarget> selected_;
};
}
