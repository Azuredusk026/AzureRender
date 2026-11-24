#pragma once
#include "runtime/SceneDocument.hpp"
#include "ecs/Components.hpp"
#include "ecs/World.hpp"
#include <map>
#include <memory>
#include <utility>

namespace azurerender {
class RuntimeLifecycle;
// Output documents keep stable addresses for this owner-thread cache lifetime.
// exchange() transfers document storage together with its preparation state.
class RuntimeSceneSnapshotCache final {
public:
    void exchange(SceneDocument& first,SceneDocument& second){
        if(&first==&second)return;
        auto& a=outputs_[&first];auto& b=outputs_[&second];
        std::swap(first,second);std::swap(a,b);
    }
    std::shared_ptr<const void> sourceIdentity(const SceneDocument& output) const {
        const auto found=outputs_.find(&output);
        return found==outputs_.end()?std::shared_ptr<const void>{}:found->second.source;
    }
    void clear() noexcept {outputs_.clear();}
private:
    friend class RuntimeLifecycle;
    struct Node {
        ecs::EntityHandle entity,parent;
        ecs::TransformComponent authored;
        bool visible=true;
    };
    struct Output {
        std::shared_ptr<const void> source;
        std::vector<Node> nodes;
        std::vector<ecs::EntityHandle> lights;
    };
    std::map<const SceneDocument*,Output> outputs_;
};
}
