#include "runtime/GameRuntime.hpp"
#include "runtime/GameComponents.hpp"
#include <chrono>
namespace azurerender {
double GameRuntime::advance(double delta) {
    lastStepSamples_.clear();
    if (!std::isfinite(delta) || delta < 0) throw std::invalid_argument("Invalid game frame delta");
    constexpr double fixed = 1.0 / 60.0;
    if (runtime_.state() == RuntimeLifecycle::State::Paused) {
        accumulator_ = 0;
        if (!runtime_.stepPending()) return 0;
        delta = fixed;
    }
    if (runtime_.state() != RuntimeLifecycle::State::Running && runtime_.state() != RuntimeLifecycle::State::Paused) return 0;
    if(sceneRevision_!=runtime_.sceneRevision()) { velocities_.clear(); previousTransforms_.clear();interpolationReady_=false;
        accumulator_=0; if(sceneRevision_!=0){input_.setFocused(false); input_.setFocused(true);}
        hasCamera_=false; interactions_.reset(); sceneRevision_=runtime_.sceneRevision();
        runtime_.world().each<game::ThirdPersonCamera>([&](auto,const auto& settings){if(!hasCamera_){cameraSettings_=settings;
            const auto target=runtime_.entity(settings.target);const auto* transform=runtime_.world().tryGet<ecs::TransformComponent>(target);
            if(transform){camera_.reset(transform->translation,settings);hasCamera_=true;cameraYaw_=camera_.yaw();}}});
    }
    accumulator_ += std::min(delta, 0.25);
    double simulated = 0;
    for (unsigned count = 0; accumulator_ + 1e-12 >= fixed && count < 15; ++count) {
        const auto start = std::chrono::steady_clock::now();
        const auto elapsed = runtime_.beginFrame(fixed);
        if (!elapsed) break;
        previousTransforms_.clear();
        runtime_.world().each<ecs::TransformComponent>([&](auto entity,const auto& transform){previousTransforms_[runtime_.nodeId(entity)]=transform;});
        previousCameraPosition_=camera_.position();previousCameraTarget_=camera_.target();interpolationReady_=true;
        motions_.clear();
        if (beforeStep_) beforeStep_(fixed);
        std::map<ecs::Entity, CharacterMotion> motions;
        std::set<ecs::Entity> active;
        runtime_.world().each<game::Character>([&](auto entity, const auto& settings) {
            active.insert(entity);
            const float x=settings.controlled?static_cast<float>(input_.down("move-right"))-static_cast<float>(input_.down("move-left")):0;
            const float z=settings.controlled?static_cast<float>(input_.down("move-back"))-static_cast<float>(input_.down("move-forward")):0;
            const float length=std::max(1.0F,std::sqrt(x*x+z*z));
            const float radians=cameraYaw_*.017453292519943295F;
            const float targetX=(x*std::cos(radians)-z*std::sin(radians))/length*settings.speed;
            const float targetZ=(x*std::sin(radians)+z*std::cos(radians))/length*settings.speed;
            auto& velocity=velocities_[entity];
            const float dx=targetX-velocity[0],dz=targetZ-velocity[2],distance=std::sqrt(dx*dx+dz*dz);
            const float amount=(x==0&&z==0?settings.braking:settings.acceleration)*static_cast<float>(fixed);
            const float factor=distance>0?std::min(1.0F,amount/distance):0;
            velocity[0]+=dx*factor;velocity[2]+=dz*factor;
            motions[entity]={velocity[0],velocity[2],settings.controlled&&input_.pressed("jump"),true};
            if(auto* transform=runtime_.world().tryGet<ecs::TransformComponent>(entity)) {
                if(std::hypot(velocity[0],velocity[2])>.01F){
                    const float desired=std::atan2(velocity[0],velocity[2])*57.295779513F-settings.forwardYaw;
                    const float difference=std::remainder(desired-transform->rotation[1],360.0F);
                    const float limit=settings.turnSpeed*static_cast<float>(fixed);
                    transform->rotation[1]=std::remainder(transform->rotation[1]+std::clamp(difference,-limit,limit),360.0F);
                }
            }
        });
        for(auto it=velocities_.begin();it!=velocities_.end();)if(!active.count(it->first))it=velocities_.erase(it);else ++it;
        for (const auto& motion : motions_) motions[motion.first] = motion.second;
        const auto events = physics_.step(runtime_, static_cast<float>(fixed), motions);
        runtime_.world().each<game::Character>([&](auto entity,const auto&) {
            const auto velocity=physics_.velocity(entity); velocities_[entity]=velocity;
            if(auto* animator=runtime_.world().tryGet<game::Animator>(entity))
                if(animator->locomotion){const float speed=std::hypot(velocity[0],velocity[2]);
                    animator->state=speed>.1F?"walk":"idle";
                    animator->playbackRate=animator->state=="walk"?speed/animator->referenceSpeed:1;}
        });
        updateCamera(static_cast<float>(fixed));
        ecs::Entity actor=ecs::kInvalidEntity;std::string actorNode;
        runtime_.world().each<game::Character>([&](auto entity,const auto& settings){
            const auto node=runtime_.nodeId(entity);
            if(settings.controlled&&!node.empty()&&(actorNode.empty()||node<actorNode)){actor=entity;actorNode=node;}});
        const auto target=interactions_.select(runtime_,physics_,actor);
        if(target&&input_.pressed("interact")&&interactionHandler_)interactionHandler_(*target);
        for (const auto& event : events) if (eventHandler_) eventHandler_(event);
        input_.endStep(); accumulator_ -= fixed; simulated += fixed; ++steps_;
        const double stepMilliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
        simulationMilliseconds_+=stepMilliseconds;lastStepSamples_.push_back(stepMilliseconds);
    }
    return simulated;
}
void GameRuntime::cameraInput(float x,float y,float scroll) {
    if(!hasCamera_||!input_.focused()||runtime_.state()!=RuntimeLifecycle::State::Running)return;
    camera_.orbit(x,y,cameraSettings_);camera_.zoom(scroll,cameraSettings_);cameraYaw_=camera_.yaw();
}
void GameRuntime::updateCamera(float dt) {
    if(!hasCamera_)return;
    const auto target=runtime_.entity(cameraSettings_.target);
    const auto* transform=runtime_.world().tryGet<ecs::TransformComponent>(target);
    if(!transform){hasCamera_=false;return;}
    camera_.update(transform->translation,cameraSettings_,dt,[&](auto origin,auto displacement,float radius){
        const auto hit=physics_.sphereSweep(origin,displacement,radius,target);return hit?hit->fraction:1.0F;});
}
float GameRuntime::renderAlpha() const {
    return runtime_.state()==RuntimeLifecycle::State::Running&&interpolationReady_
        ? static_cast<float>(std::clamp(accumulator_*60,0.0,1.0)):1;
}
SceneDocument GameRuntime::renderScene() {
    auto scene=runtime_.snapshotScene();const float alpha=renderAlpha();
    for(auto& node:scene.nodes){const auto previous=previousTransforms_.find(node.id);if(previous==previousTransforms_.end())continue;
        for(unsigned axis=0;axis<3;++axis){
            node.translation[axis]=previous->second.translation[axis]+(node.translation[axis]-previous->second.translation[axis])*alpha;
            node.rotation[axis]=previous->second.rotation[axis]+std::remainder(node.rotation[axis]-previous->second.rotation[axis],360.0F)*alpha;
        }
    }
    return scene;
}
std::array<float,3> GameRuntime::renderCameraPosition() const {
    auto position=camera_.position();const float alpha=renderAlpha();
    for(unsigned axis=0;axis<3;++axis)position[axis]=previousCameraPosition_[axis]+(position[axis]-previousCameraPosition_[axis])*alpha;
    return position;
}
std::array<float,3> GameRuntime::renderCameraTarget() const {
    auto target=camera_.target();const float alpha=renderAlpha();
    for(unsigned axis=0;axis<3;++axis)target[axis]=previousCameraTarget_[axis]+(target[axis]-previousCameraTarget_[axis])*alpha;
    return target;
}

}
