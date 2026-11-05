#include <iostream>
#include <stdexcept>

#include "runtime/RuntimeLifecycle.hpp"
#include "scene/SceneDescription.hpp"
using azurerender::RuntimeLifecycle;
void require(bool v, const char* message = "Lifecycle assertion failed") {
    if (!v) throw std::runtime_error(message);
}
void checkNodeIdentityLookup() {
    RuntimeLifecycle runtime;
    require(runtime.nodeId(azurerender::ecs::kInvalidEntity).empty(), "Invalid entity has no node identity");
    azurerender::SceneDocument document;
    for (const auto* id : {"first", "second"}) {
        azurerender::SceneNode node;
        node.id = id;
        document.nodes.push_back(node);
    }
    runtime.loadScene(document);
    const auto first = runtime.entity("first");
    const auto second = runtime.entity("second");
    require(runtime.nodeId(first) == "first" && runtime.nodeId(second) == "second",
        "Live node identities must resolve through the owning runtime");
    const auto anonymous = runtime.world().createEntity();
    require(runtime.nodeId(anonymous).empty(), "Anonymous ECS entities have no document identity");

    RuntimeLifecycle other;
    auto foreign = document;
    foreign.nodes.resize(1);
    foreign.nodes[0].id = "foreign";
    other.loadScene(foreign);
    runtime.world().swap(other.world());
    require(runtime.nodeId(first).empty() && other.nodeId(first).empty(),
        "An identity component from another document must reject despite matching numeric entity IDs");
    runtime.world().swap(other.world());
    require(runtime.nodeId(first) == "first", "Restored ownership must resolve the original node");

    runtime.world().destroyEntity(first);
    const auto recycled = runtime.world().createEntity();
    require(recycled == first && runtime.nodeId(recycled).empty(),
        "A recycled entity must not inherit the destroyed document node identity");
    require(runtime.entity("first") == azurerender::ecs::kInvalidEntity,
        "The forward lookup must also reject a recycled anonymous entity");
    auto replacement = foreign;
    replacement.nodes[0].id = "replacement";
    runtime.replaceScene(replacement);
    const auto current = runtime.entity("replacement");
    require(runtime.nodeId(current) == "replacement" && runtime.entity("second") == azurerender::ecs::kInvalidEntity,
        "Scene replacement must publish only the replacement document identities");
    bool rejected = false;
    try {
        runtime.replaceScene(document, [](RuntimeLifecycle&) { throw std::runtime_error("configuration rejected"); });
    } catch (const std::runtime_error&) { rejected = true; }
    require(rejected && runtime.nodeId(current) == "replacement",
        "Failed scene replacement must preserve the original identity ownership");
    runtime.start();
    azurerender::SceneNode spawned;
    spawned.id = "spawned";
    runtime.deferSpawn(spawned);
    require(runtime.entity("spawned") == azurerender::ecs::kInvalidEntity,
        "Deferred nodes acquire identities at their commit boundary");
    runtime.beginFrame(.1);
    require(runtime.nodeId(runtime.entity("spawned")) == "spawned", "Committed spawns must resolve their identity");
    runtime.stop();
    require(runtime.nodeId(current).empty(), "Stopped worlds release node identities");
}

int main() {
    checkNodeIdentityLookup();
    RuntimeLifecycle runtime;
    int updates = 0;
    azurerender::SceneDocument document;
    azurerender::SceneNode node;
    node.id = "root";
    document.nodes.push_back(node);
    document.lights.push_back({"light", "root"});
    runtime.loadScene(document);
    require(runtime.snapshotScene().nodes.size() == 1);
    auto nodes = runtime.world().componentArray<azurerender::ecs::TransformComponent>().entities();
    require(nodes.size() == 1);
    runtime.world().tryGet<azurerender::ecs::TransformComponent>(nodes[0])->translation[0] = 3;
    require(runtime.snapshotScene().nodes[0].translation[0] == 3);
    runtime.world().tryGet<azurerender::ecs::RenderableComponent>(nodes[0])->visible = false;
    const auto renderScene = runtime.snapshotScene().renderDescription();
    require(renderScene.nodes[0].translation[0] == 3);
    require(!renderScene.nodes[0].visible);
    require(renderScene.lights[0].position[0] == 3);
    runtime.world().addSystem([&](azurerender::ecs::World&) { ++updates; });
    runtime.world().destroyEntity(nodes[0]);
    auto replacement = runtime.world().createEntity();
    require(replacement == nodes[0]);
    require(runtime.snapshotScene().nodes.empty());
    const auto deletedScene = runtime.snapshotScene().renderDescription();
    require(deletedScene.nodes.empty() && deletedScene.lights.empty());
    runtime.start();
    require(runtime.beginFrame(0.1) == 0.1);
    require(updates == 1);
    runtime.pause();
    require(runtime.beginFrame(0.1) == 0);
    require(updates == 1);
    runtime.step();
    require(runtime.beginFrame(0.1) == 0.1);
    require(updates == 2);
    require(runtime.beginFrame(0.1) == 0);
    require(updates == 2);
    auto entity = runtime.world().createEntity();
    runtime.defer([entity](auto& w) { w.destroyEntity(entity); });
    runtime.beginFrame(0.1);
    require(!runtime.world().valid(entity));
    runtime.resume();
    runtime.beginFrame(0.1);
    require(updates == 3);
    bool invalid = false;
    try {
        runtime.beginFrame(-1);
    } catch (const std::invalid_argument&) {
        invalid = true;
    }
    require(invalid);
    runtime.stop();
    require(runtime.beginFrame(0.1) == 0);
    require(!runtime.world().valid(nodes[0]));
    bool closed = false;
    try {
        runtime.defer([](auto&) {});
    } catch (const std::logic_error&) {
        closed = true;
    }
    require(closed);
    std::cout << "Runtime lifecycle contracts passed\n";
}
