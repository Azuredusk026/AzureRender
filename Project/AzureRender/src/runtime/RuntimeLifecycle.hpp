#pragma once
#include <cmath>
#include <functional>
#include <iterator>
#include <map>
#include <set>
#include <stdexcept>
#include <vector>

#include "SceneDocument.hpp"
#include "ecs/Components.hpp"
#include "ecs/World.hpp"
namespace azurerender {
class RuntimeLifecycle final {
   public:
    enum class State { Created, Running, Paused, Stopped };
    ecs::World& world() noexcept { return world_; }
    const ecs::World& world() const noexcept { return world_; }
    State state() const noexcept { return state_; }
    std::uint64_t sceneRevision() const noexcept { return sceneRevision_; }
    bool stepPending() const noexcept { return step_; }
    ecs::Entity entity(const std::string& nodeId) {
        return nodeValid(nodeId) ? nodes_.at(nodeId) : ecs::kInvalidEntity;
    }
    std::string nodeId(ecs::Entity entityId) {
        for (const auto& node : nodes_) if (node.second == entityId && nodeValid(node.first)) return node.first;
        return {};
    }
    void replaceScene(const SceneDocument& scene, const std::function<void(RuntimeLifecycle&)>& configure = {}) {
        if (state_ == State::Stopped) throw std::logic_error("Cannot replace a stopped runtime");
        RuntimeLifecycle candidate;
        candidate.loadScene(scene);
        if (configure) configure(candidate);
        world_.swap(candidate.world_);
        nodes_.swap(candidate.nodes_);
        std::swap(scene_, candidate.scene_);
        pending_.clear();
        pendingSpawns_.clear();
        ++sceneRevision_;
    }
    void start() {
        if (state_ != State::Created) throw std::logic_error("Runtime start requires Created");
        state_ = State::Running;
    }
    void pause() {
        if (state_ != State::Running) throw std::logic_error("Runtime pause requires Running");
        state_ = State::Paused;
    }
    void resume() {
        if (state_ != State::Paused) throw std::logic_error("Runtime resume requires Paused");
        step_ = false;
        state_ = State::Running;
    }
    void step() {
        if (state_ != State::Paused) throw std::logic_error("Runtime step requires Paused");
        step_ = true;
    }
    void stop() noexcept {
        pending_.clear();
        pendingSpawns_.clear();
        world_.clear();
        nodes_.clear();
        step_ = false;
        state_ = State::Stopped;
    }
    void loadScene(const SceneDocument& scene) {
        if (state_ != State::Created)
            throw std::logic_error("Scene initialization requires Created");
        if (!nodes_.empty()) throw std::logic_error("Runtime scene is already initialized");
        std::map<std::string, std::string> parents;
        for (const auto& node : scene.nodes)
            if (node.id.empty() || !parents.emplace(node.id, node.parentId).second)
                throw std::invalid_argument("Duplicate or empty runtime node id");
        for (const auto& node : scene.nodes) {
            auto parent = node.parentId;
            std::size_t depth = 0;
            while (!parent.empty()) {
                if (!parents.count(parent))
                    throw std::invalid_argument("Unknown runtime parent: " + parent);
                if (++depth > parents.size())
                    throw std::invalid_argument("Runtime hierarchy cycle");
                parent = parents.at(parent);
            }
        }
        scene_ = scene;
        ++sceneRevision_;
        for (const auto& node : scene.nodes) {
            if (node.id.empty() || nodes_.count(node.id))
                throw std::invalid_argument("Duplicate or empty runtime node id");
            const auto entity = world_.createEntity();
            nodes_.emplace(node.id, entity);
            world_.addComponent(entity, NodeIdentity{node.id});
            world_.addComponent(
                entity, ecs::TransformComponent{node.translation, node.rotation, node.scale});
            world_.addComponent(entity, ecs::RenderableComponent{0, node.visible});
        }
    }
    SceneDocument snapshotScene() {
        SceneDocument result = scene_;
        result.nodes.clear();
        for (auto node : scene_.nodes) {
            const auto entity = nodes_.at(node.id);
            if (!nodeValid(node.id)) continue;
            if (const auto* transform = world_.tryGet<ecs::TransformComponent>(entity)) {
                node.translation = transform->translation;
                node.rotation = transform->rotation;
                node.scale = transform->scale;
            }
            if (const auto* renderable = world_.tryGet<ecs::RenderableComponent>(entity))
                node.visible = renderable->visible;
            if (!node.parentId.empty() && !nodeValid(node.parentId)) node.parentId.clear();
            result.nodes.push_back(std::move(node));
        }
        result.lights.erase(
            std::remove_if(result.lights.begin(), result.lights.end(),
                           [&](const auto& light) { return !nodeValid(light.nodeId); }),
            result.lights.end());
        return result;
    }
    void defer(ecs::System operation) {
        if (state_ == State::Stopped) throw std::logic_error("Runtime is stopped");
        if (!operation) throw std::invalid_argument("Empty deferred operation");
        pending_.push_back(std::move(operation));
    }
    // Builder may enqueue only. Restore the entire queue and spawn reservations
    // when validation or allocation fails before the next frame consumes it.
    void transactionalQueue(const std::function<void()>& builder) {
        auto operations=pending_;auto reservations=pendingSpawns_;
        try{builder();}catch(...){pending_.swap(operations);pendingSpawns_.swap(reservations);throw;}
    }
    void validateSpawn(const SceneNode& node) {
        if(node.id.empty()||node.id.size()>128||nodes_.count(node.id)||pendingSpawns_.count(node.id))
            throw std::invalid_argument("Spawn identity is empty, too long or already reserved");
        if(nodes_.size()+pendingSpawns_.size()>=4096)throw std::invalid_argument("Spawn node capacity exceeded");
        if(!node.parentId.empty()&&!nodeValid(node.parentId))throw std::invalid_argument("Spawn parent is stale");
        if(!node.resourceId.empty()&&std::none_of(scene_.resources.begin(),scene_.resources.end(),[&](const auto& r){return r.id==node.resourceId;}))
            throw std::invalid_argument("Spawn resource is unavailable");
    }
    void deferSpawn(SceneNode node,std::function<void(ecs::World&,ecs::Entity)> configure={}) {
        validateSpawn(node);const auto id=node.id;
        defer([this,node=std::move(node),configure=std::move(configure)](auto& world){
            pendingSpawns_.erase(node.id);
            if(nodes_.count(node.id))throw std::invalid_argument("Spawn identity already exists: "+node.id);
            if(!node.parentId.empty()&&!nodeValid(node.parentId))throw std::invalid_argument("Spawn parent is stale");
            if(!node.resourceId.empty()&&std::none_of(scene_.resources.begin(),scene_.resources.end(),[&](const auto& r){return r.id==node.resourceId;}))
                throw std::invalid_argument("Spawn resource is unavailable");
            auto nodes=scene_.nodes;nodes.push_back(node);
            const auto entity=world.createEntity();
            try {
                world.addComponent(entity,NodeIdentity{node.id});
                world.addComponent(entity,ecs::TransformComponent{node.translation,node.rotation,node.scale});
                world.addComponent(entity,ecs::RenderableComponent{0,node.visible});
                if(configure)configure(world,entity);
                nodes_.emplace(node.id,entity);scene_.nodes.swap(nodes);
            }catch(...){world.destroyEntity(entity);throw;}
        });
        pendingSpawns_.insert(id);
    }
    double beginFrame(double delta) {
        if (!std::isfinite(delta) || delta < 0)
            throw std::invalid_argument("Runtime delta must be finite and nonnegative");
        if (state_ == State::Created || state_ == State::Stopped) return 0;
        auto operations = std::move(pending_);
        pending_.clear();
        const auto batchRevision=sceneRevision_;
        for (std::size_t index=0;index<operations.size();++index) {
            try{operations[index](world_);}
            catch(...){
                if(sceneRevision_==batchRevision&&state_!=State::Stopped)
                    pending_.insert(pending_.begin(),std::make_move_iterator(operations.begin()+index+1),std::make_move_iterator(operations.end()));
                throw;
            }
            if(sceneRevision_!=batchRevision||state_==State::Stopped)return 0;
        }
        if (state_ == State::Paused && !step_) return 0;
        step_ = false;
        world_.update();
        return delta;
    }

   private:
    struct NodeIdentity {
        std::string id;
    };
    bool nodeValid(const std::string& id) {
        const auto found = nodes_.find(id);
        if (found == nodes_.end() || !world_.valid(found->second)) return false;
        const auto* identity = world_.tryGet<NodeIdentity>(found->second);
        return identity && identity->id == id;
    }
    std::uint64_t sceneRevision_ = 0;
    SceneDocument scene_;
    std::map<std::string, ecs::Entity> nodes_;
    ecs::World world_;
    State state_ = State::Created;
    bool step_ = false;
    std::vector<ecs::System> pending_;
    std::set<std::string> pendingSpawns_;
};
}  // namespace azurerender
