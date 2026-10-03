#include <iostream>
#include <stdexcept>

#include "runtime/RuntimeLifecycle.hpp"
#include "scene/SceneDescription.hpp"
using azurerender::RuntimeLifecycle;
void require(bool v) {
    if (!v) throw std::runtime_error("Lifecycle assertion failed");
}
int main() {
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
