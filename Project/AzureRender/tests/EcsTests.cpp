#include "ecs/ComponentArray.hpp"
#include "ecs/Entity.hpp"
#include "ecs/World.hpp"

#include <cassert>
#include <string>
#include <vector>

namespace {

struct TransformComponent {
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
};

struct TagComponent {
    std::string label;
};

int g_updateCount = 0;
int g_lastTaggedCount = 0;

void countTaggedEntities(azurerender::ecs::World& world) {
    ++g_updateCount;
    g_lastTaggedCount = 0;
    auto& tags = world.componentArray<TagComponent>();
    for (const auto& entry : tags.components()) {
        if (!entry.second.label.empty()) {
            ++g_lastTaggedCount;
        }
    }
}

}  // namespace

int main() {
    using azurerender::ecs::Entity;
    using azurerender::ecs::World;

    World world;
    const Entity e1 = world.createEntity();
    const Entity e2 = world.createEntity();
    const Entity e3 = world.createEntity();
    assert(e1 != azurerender::ecs::kInvalidEntity);
    assert(world.entityCount() == 3);

    world.addComponent(e1, TransformComponent{1.0F, 2.0F, 3.0F});
    world.addComponent(e2, TransformComponent{4.0F, 5.0F, 6.0F});
    world.addComponent(e2, TagComponent{"primary"});
    world.addComponent(e3, TagComponent{"shadow"});

    auto& transforms = world.componentArray<TransformComponent>();
    assert(transforms.size() == 2);
    TransformComponent* t2 = world.tryGet<TransformComponent>(e2);
    assert(t2 != nullptr);
    assert(t2->x == 4.0F);

    assert(world.has<TagComponent>(e2));
    assert(!world.has<TransformComponent>(e3));

    g_updateCount = 0;
    g_lastTaggedCount = 0;
    world.addSystem(countTaggedEntities);
    world.update();
    assert(g_updateCount == 1);
    assert(g_lastTaggedCount == 2);
    world.update();
    assert(g_updateCount == 2);

    world.destroyEntity(e2);
    assert(!world.has<TagComponent>(e2));
    assert(transforms.size() == 1);
    assert(world.entityCount() == 2);

    const Entity e4 = world.createEntity();
    assert(e4 == e2);
    assert(world.has<TagComponent>(e4) == false);

    return 0;
}