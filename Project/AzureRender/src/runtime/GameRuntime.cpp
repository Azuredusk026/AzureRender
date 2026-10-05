#include "runtime/GameRuntime.hpp"
#include "runtime/GameComponents.hpp"
#include <chrono>
namespace azurerender {
RuntimeSystemContext GameRuntime::systemContext() { return {runtime_,input_,physics_,*this,1.0/60.0}; }
GameRuntime::GameRuntime(RuntimeLifecycle& runtime):runtime_(runtime) {}
GameRuntime::GameRuntime(RuntimeLifecycle& runtime,const SystemRegistry& registry,const nlohmann::json& config):runtime_(runtime) {
    if(!config.is_object() || !config.contains("schemaVersion") || !config.at("schemaVersion").is_number_integer() || config.at("schemaVersion")!=1)
        throw std::invalid_argument("Unsupported runtime configuration");
    for(const auto& item:config.items())if(item.key()!="schemaVersion"&&item.key()!="input"&&item.key()!="systems")
        throw std::invalid_argument("Unknown runtime configuration field: " + item.key());
    input_.configure(config.at("input"));
    const auto& descriptions=config.at("systems");
    if(!descriptions.is_array() || descriptions.size()>128)throw std::invalid_argument("Invalid system list");
    std::set<std::string> ids;
    for(const auto& description:descriptions) {
        if(!description.is_object())throw std::invalid_argument("Invalid system descriptor");
        for(const auto& item:description.items())if(item.key()!="id"&&item.key()!="config")throw std::invalid_argument("Unknown system descriptor field");
        const auto id=description.at("id").get<std::string>();
        if(!ids.insert(id).second)throw std::invalid_argument("Duplicate system instance: " + id);
        systems_.push_back(registry.create(id,description.value("config",nlohmann::json::object())));
    }
    auto context=systemContext();std::size_t initialized=0;
    try { for(auto& system:systems_) { ++initialized; system->initialize(context); } }
    catch(...) { while(initialized)systems_[--initialized]->shutdown(context);throw; }
}
GameRuntime::~GameRuntime() {
    auto context=systemContext();for(auto it=systems_.rbegin();it!=systems_.rend();++it)(*it)->shutdown(context);
}
void GameRuntime::synchronizeSystemScene() {
    previousTransforms_.clear();interpolationReady_=false;
    if(sceneReady_)input_.release();
    sceneReady_=true;hasCamera_=false;interactions_.reset();sceneRevision_=runtime_.sceneRevision();
    auto context=systemContext();for(auto& system:systems_)system->sceneChanged(context);
}

double GameRuntime::advance(double delta) {
    lastStepSamples_.clear();
    if (!std::isfinite(delta) || delta < 0) throw std::invalid_argument("Invalid game frame delta");
    constexpr double fixed = 1.0 / 60.0;
    if (runtime_.state() == RuntimeLifecycle::State::Paused) {
        accumulator_ = 0;
        if (!runtime_.stepPending()) { input_.release(); return 0; }
        delta = fixed;
    }
    if (runtime_.state() != RuntimeLifecycle::State::Running && runtime_.state() != RuntimeLifecycle::State::Paused) return 0;
    if(!sceneReady_ || sceneRevision_!=runtime_.sceneRevision()) {
        accumulator_=0;synchronizeSystemScene();
    }
    accumulator_ += std::min(delta, 0.25);
    double simulated = 0;
    for (unsigned count = 0; accumulator_ + 1e-12 >= fixed && count < 15; ++count) {
        const auto start = std::chrono::steady_clock::now();
        const auto elapsed = runtime_.beginFrame(fixed);
        if(sceneRevision_!=runtime_.sceneRevision())synchronizeSystemScene();
        if (!elapsed) { accumulator_=0; break; }
        previousTransforms_.clear();
        runtime_.world().each<ecs::TransformComponent>([&](auto entity,const auto& transform){previousTransforms_[runtime_.nodeId(entity)]=transform;});
        previousCameraPosition_=camera_.position();previousCameraTarget_=camera_.target();interpolationReady_=true;
        motions_.clear();
        if (beforeStep_) beforeStep_(fixed);
        events_.clear();
        auto context=systemContext(); for(auto& system:systems_)system->fixedStep(context);
        for (const auto& event : events_) if (eventHandler_) eventHandler_(event);
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
void GameRuntime::activateCamera(const game::ThirdPersonCamera& settings,std::array<float,3> target) {
    cameraSettings_=settings;camera_.reset(target,settings);hasCamera_=true;cameraYaw_=camera_.yaw();
}
void GameRuntime::updateCamera(std::array<float,3> target,double dt,ecs::Entity ignored) {
    if(!hasCamera_)return;
    camera_.update(target,cameraSettings_,static_cast<float>(dt),[&](auto origin,auto displacement,float radius) {
        const auto hit=physics_.sphereSweep(origin,displacement,radius,ignored);return hit?hit->fraction:1.0F;
    });
}
void GameRuntime::selectInteraction(ecs::Entity actor,const std::string& action) {
    const auto target=interactions_.select(runtime_,physics_,actor);
    if(target&&input_.pressed(action)&&interactionHandler_)interactionHandler_(*target);
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
