#pragma once
#include <cmath>
#include <functional>
#include <map>
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
    State state() const noexcept { return state_; }
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
    double beginFrame(double delta) {
        if (!std::isfinite(delta) || delta < 0)
            throw std::invalid_argument("Runtime delta must be finite and nonnegative");
        if (state_ == State::Created || state_ == State::Stopped) return 0;
        auto operations = std::move(pending_);
        pending_.clear();
        for (auto& operation : operations) operation(world_);
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
    SceneDocument scene_;
    std::map<std::string, ecs::Entity> nodes_;
    ecs::World world_;
    State state_ = State::Created;
    bool step_ = false;
    std::vector<ecs::System> pending_;
};
}  // namespace azurerender
