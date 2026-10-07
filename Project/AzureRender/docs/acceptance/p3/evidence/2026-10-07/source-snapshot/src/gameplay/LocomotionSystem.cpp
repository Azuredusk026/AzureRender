#include "gameplay/SharedSystems.hpp"
#include "gameplay/SystemConfiguration.hpp"
#include "runtime/GameRuntime.hpp"
#include <cmath>
namespace azurerender::gameplay {
namespace {
struct States { std::string idle,moving;float threshold; };
class Locomotion final: public IRuntimeSystem {
    std::map<std::string,States> profiles_;
public:
    explicit Locomotion(const nlohmann::json& config) {
        fields(config,{"profiles"});const auto& profiles=config.at("profiles");
        if(!profiles.is_object()||profiles.empty())throw std::invalid_argument("Locomotion profiles must be a nonempty object");
        for(const auto& p:profiles.items()) {
            if(p.key().empty())throw std::invalid_argument("Empty locomotion profile");
            fields(p.value(),{"idle","moving","threshold"});const float threshold=p.value().value("threshold",.1F);
            if(!std::isfinite(threshold)||threshold<0||threshold>100)throw std::invalid_argument("Invalid locomotion threshold");
            profiles_[p.key()]={text(p.value(),"idle"),text(p.value(),"moving"),threshold};
        }
    }
    void fixedStep(RuntimeSystemContext& c) override {
        c.runtime.world().each<game::Character,game::Animator>([&](auto entity,const auto&,auto& animator) {
            if(!animator.locomotion)return;
            const auto found=profiles_.find(animator.locomotionProfile);
            if(found==profiles_.end())throw std::invalid_argument("Unknown locomotion profile: " + animator.locomotionProfile);
            const auto velocity=c.physics.velocity(entity);const float speed=std::hypot(velocity[0],velocity[2]);
            const bool moving=speed>found->second.threshold;
            animator.state=moving?found->second.moving:found->second.idle;
            animator.playbackRate=moving?speed/animator.referenceSpeed:1;
        });
    }
};
}
std::unique_ptr<IRuntimeSystem> locomotionSystem(const nlohmann::json& config) { return std::make_unique<Locomotion>(config); }
}
