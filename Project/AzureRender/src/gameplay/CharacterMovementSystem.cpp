#include "gameplay/SharedSystems.hpp"
#include "gameplay/SystemConfiguration.hpp"
#include "runtime/GameRuntime.hpp"
#include <cmath>
namespace azurerender::gameplay {
namespace {
struct Actions { std::string left,right,forward,back,jump,sprint; };
class CharacterMovement final: public IRuntimeSystem {
    std::map<std::string,Actions> profiles_;
public:
    explicit CharacterMovement(const nlohmann::json& config) {
        fields(config,{"profiles"});const auto& profiles=config.at("profiles");
        if(!profiles.is_object()||profiles.empty())throw std::invalid_argument("Movement profiles must be a nonempty object");
        for(const auto& p:profiles.items()) {
            if(p.key().empty())throw std::invalid_argument("Empty movement profile");
            fields(p.value(),{"left","right","forward","back","jump","sprint"});
            profiles_[p.key()]={text(p.value(),"left"),text(p.value(),"right"),text(p.value(),"forward"),text(p.value(),"back"),text(p.value(),"jump"),text(p.value(),"sprint")};
        }
    }
    void initialize(RuntimeSystemContext& c) override {
        for(const auto& profile:profiles_) {
            const auto& a=profile.second;
            for(const auto& action:{a.left,a.right,a.forward,a.back,a.jump,a.sprint})
                if(!c.input.hasAction(action))throw std::invalid_argument("Unbound movement action: " + action);
        }
    }
    void fixedStep(RuntimeSystemContext& c) override {
        c.runtime.world().each<game::Character>([&](auto entity,const auto& settings) {
            const auto found=profiles_.find(settings.inputProfile);
            if(found==profiles_.end())throw std::invalid_argument("Unknown movement profile: " + settings.inputProfile);
            const auto& actions=found->second;
            const float x=settings.controlled?static_cast<float>(c.input.down(actions.right))-static_cast<float>(c.input.down(actions.left)):0;
            const float z=settings.controlled?static_cast<float>(c.input.down(actions.back))-static_cast<float>(c.input.down(actions.forward)):0;
            const float length=std::max(1.0F,std::sqrt(x*x+z*z));
            const float radians=c.game.cameraYaw()*.017453292519943295F;
            const float speed=settings.speed*(settings.controlled&&c.input.down(actions.sprint)?settings.sprintMultiplier:1.0F);
            const float targetX=(x*std::cos(radians)-z*std::sin(radians))/length*speed;
            const float targetZ=(x*std::sin(radians)+z*std::cos(radians))/length*speed;
            auto velocity=c.physics.velocity(entity);
            const float dx=targetX-velocity[0],dz=targetZ-velocity[2],distance=std::hypot(dx,dz);
            const float amount=(x==0&&z==0?settings.braking:settings.acceleration)*static_cast<float>(c.fixedStep);
            const float factor=distance>0?std::min(1.0F,amount/distance):0;
            velocity[0]+=dx*factor;velocity[2]+=dz*factor;
            c.game.stageMotion(entity,{velocity[0],velocity[2],settings.controlled&&c.input.pressed(actions.jump),true});
            if(auto* transform=c.runtime.world().tryGet<ecs::TransformComponent>(entity)) {
                if(std::hypot(velocity[0],velocity[2])>.01F) {
                    const float desired=std::atan2(velocity[0],velocity[2])*57.295779513F-settings.forwardYaw;
                    const float difference=std::remainder(desired-transform->rotation[1],360.0F);
                    const float limit=settings.turnSpeed*static_cast<float>(c.fixedStep);
                    transform->rotation[1]=std::remainder(transform->rotation[1]+std::clamp(difference,-limit,limit),360.0F);
                }
            }
        });
    }
};
}
std::unique_ptr<IRuntimeSystem> characterMovementSystem(const nlohmann::json& config) { return std::make_unique<CharacterMovement>(config); }
}
