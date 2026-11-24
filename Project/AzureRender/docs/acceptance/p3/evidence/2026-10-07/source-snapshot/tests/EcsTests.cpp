#include "ecs/ComponentArray.hpp"
#include "ecs/Components.hpp"
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
    for (const TagComponent& tag : tags.dense()) {
        if (!tag.label.empty()) {
            ++g_lastTaggedCount;
        }
    }
}

}  // namespace

int main() {
    using azurerender::ecs::Entity;
    using azurerender::ecs::World;

    {
        World lifecycle;
        auto removed=lifecycle.createEntity();lifecycle.destroyEntity(removed);lifecycle.destroyEntity(removed);
        auto first=lifecycle.createEntity();auto second=lifecycle.createEntity();
        if(first==second||lifecycle.entityCount()!=2)return 1;
    }
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

    // TransformComponent / RenderableComponent + each() traversal.
    world.addComponent(e1, azurerender::ecs::TransformComponent{
        {1.0F, 2.0F, 3.0F}, {0.0F, 0.0F, 0.0F}, {1.0F, 1.0F, 1.0F}});
    world.addComponent(e1, azurerender::ecs::RenderableComponent{0, true});
    world.addComponent(e3, azurerender::ecs::RenderableComponent{1, false});
    int transformCount = 0;
    int visibleCount = 0;
    world.each<azurerender::ecs::RenderableComponent>(
        [&](const azurerender::ecs::Entity,
            azurerender::ecs::RenderableComponent& component) {
            if (component.visible) {
                ++visibleCount;
            }
        });
    world.each<azurerender::ecs::TransformComponent>(
        [&](const azurerender::ecs::Entity,
            azurerender::ecs::TransformComponent&) {
            ++transformCount;
        });
    assert(visibleCount == 1);
    assert(transformCount == 1);
    auto* transform = world.tryGet<azurerender::ecs::TransformComponent>(e1);
    assert(transform != nullptr);
    assert(transform->translation[0] == 1.0F);

    // Two-component each traversal.
    int paired = 0;
    world.each<azurerender::ecs::TransformComponent,
        azurerender::ecs::RenderableComponent>(
        [&](const azurerender::ecs::Entity,
            azurerender::ecs::TransformComponent&,
            azurerender::ecs::RenderableComponent&) {
            ++paired;
        });
    assert(paired == 1);

    // Dense storage: components and their owning entities are parallel
    // contiguous arrays, and erase swap-fills the freed slot.
    World denseWorld;
    const Entity d1 = denseWorld.createEntity();
    const Entity d2 = denseWorld.createEntity();
    const Entity d3 = denseWorld.createEntity();
    denseWorld.addComponent(d1, TransformComponent{10.0F, 0.0F, 0.0F});
    denseWorld.addComponent(d2, TransformComponent{20.0F, 0.0F, 0.0F});
    denseWorld.addComponent(d3, TransformComponent{30.0F, 0.0F, 0.0F});
    auto& denseTransforms = denseWorld.componentArray<TransformComponent>();
    assert(denseTransforms.dense().size() == 3);
    assert(denseTransforms.entities().size() == 3);
    assert(denseTransforms.dense()[1].x == 20.0F);
    assert(denseTransforms.entities()[1] == d2);

    // Inserting the same entity twice updates in place.
    denseWorld.addComponent(d2, TransformComponent{21.0F, 0.0F, 0.0F});
    assert(denseTransforms.dense().size() == 3);
    assert(denseWorld.tryGet<TransformComponent>(d2)->x == 21.0F);

    // Erase d1: d3's component moves into the freed slot and stays findable.
    denseTransforms.erase(d1);
    assert(denseTransforms.dense().size() == 2);
    assert(!denseTransforms.contains(d1));
    const TransformComponent* d3After =
        denseWorld.tryGet<TransformComponent>(d3);
    assert(d3After != nullptr);
    assert(d3After->x == 30.0F);
    for (std::size_t index = 0; index < denseTransforms.dense().size();
         ++index) {
        const azurerender::ecs::Entity owner =
            denseTransforms.entities()[index];
        assert(
            denseWorld.tryGet<TransformComponent>(owner)
            == &denseTransforms.dense()[index]);
    }

    return 0;
}
