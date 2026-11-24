#include "gameplay/SharedSystems.hpp"
#include "gameplay/SystemConfiguration.hpp"
#include "runtime/GameRuntime.hpp"
namespace azurerender::gameplay {
namespace {
class CameraFollow final: public IRuntimeSystem {
    bool orbit_=false;
    std::array<float,3> target_{};
    game::ThirdPersonCamera settings_;
public:
    explicit CameraFollow(const nlohmann::json& config) {
        fields(config,{"mode","target"});const auto mode=config.value("mode",std::string("follow"));
        if(mode!="follow"&&mode!="orbit")throw std::invalid_argument("Unknown camera mode");
        orbit_=mode=="orbit";
        if(config.contains("target"))target_=config.at("target").get<std::array<float,3>>();
        for(float value:target_)if(!std::isfinite(value))throw std::invalid_argument("Invalid camera target");
        if(!orbit_&&config.contains("target"))throw std::invalid_argument("Follow camera target belongs to its component");
    }
    void sceneChanged(RuntimeSystemContext& c) override {
        c.game.deactivateCamera();
        if(orbit_) { c.game.activateCamera(settings_,target_);return; }
        // Stable node identity determines which configured camera owns the view.
        std::string selected;
        c.runtime.world().each<game::ThirdPersonCamera>([&](auto entity,const auto& settings) {
            const auto node=c.runtime.nodeId(entity);const auto target=c.runtime.entity(settings.target);
            const auto* transform=c.runtime.world().tryGet<ecs::TransformComponent>(target);
            if(transform && !node.empty() && (selected.empty()||node<selected)) {
                selected=node;settings_=settings;c.game.activateCamera(settings_,transform->translation);
            }
        });
    }
    void fixedStep(RuntimeSystemContext& c) override {
        if(orbit_) { c.game.updateCamera(target_,c.fixedStep);return; }
        if(!c.game.hasCamera())return;
        const auto target=c.runtime.entity(settings_.target);
        const auto* transform=c.runtime.world().tryGet<ecs::TransformComponent>(target);
        if(!transform) { c.game.deactivateCamera();return; }
        c.game.updateCamera(transform->translation,c.fixedStep,target);
    }
};
}
std::unique_ptr<IRuntimeSystem> cameraFollowSystem(const nlohmann::json& config) { return std::make_unique<CameraFollow>(config); }
}
