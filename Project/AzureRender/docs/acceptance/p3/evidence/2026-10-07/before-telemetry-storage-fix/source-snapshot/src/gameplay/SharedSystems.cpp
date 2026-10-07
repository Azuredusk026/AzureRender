#include "gameplay/SharedSystems.hpp"
#include "gameplay/SystemConfiguration.hpp"
#include "runtime/GameRuntime.hpp"
namespace azurerender::gameplay {
namespace {
class PhysicsSystem final: public IRuntimeSystem {
public:
    void fixedStep(RuntimeSystemContext& c) override { c.game.physicsEvents(c.physics.step(c.runtime,static_cast<float>(c.fixedStep),c.game.motions())); }
};
class InteractionSystem final: public IRuntimeSystem {
    std::string actor_,action_;
public:
    explicit InteractionSystem(const nlohmann::json& config) {
        fields(config,{"actor","action"});actor_=config.value("actor",std::string{});action_=text(config,"action");
    }
    void initialize(RuntimeSystemContext& c) override {
        if(!c.input.hasAction(action_))throw std::invalid_argument("Unbound interaction action: " + action_);
    }
    void fixedStep(RuntimeSystemContext& c) override {
        auto actor=c.runtime.entity(actor_);
        if(actor_.empty()) {
            std::string selected;
            c.runtime.world().each<game::Character>([&](auto entity,const auto& settings) {
                const auto node=c.runtime.nodeId(entity);
                if(settings.controlled&&!node.empty()&&(selected.empty()||node<selected)) { actor=entity;selected=node; }
            });
        }
        c.game.selectInteraction(actor,action_);
    }
};
}
void registerSharedSystems(SystemRegistry& registry) {
    registry.add("character-movement",characterMovementSystem);
    registry.add("physics",[](const auto& config) { fields(config,{});return std::make_unique<PhysicsSystem>(); });
    registry.add("locomotion",locomotionSystem);
    registry.add("camera",cameraFollowSystem);
    registry.add("interaction",[](const auto& config) { return std::make_unique<InteractionSystem>(config); });
}
}
