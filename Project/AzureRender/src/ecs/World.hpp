#pragma once

#include "ComponentArray.hpp"
#include "Entity.hpp"
#include "IComponentArray.hpp"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace azurerender::ecs {

// A system is just a callable invoked once per World::update.
using System = std::function<void(class World&)>;

// Owned component arrays indexed by std::type_index for fast lookup.
class World final {
public:
    World() = default;
    World(const World&) = delete;
    World& operator=(const World&) = delete;

    // Allocate a new entity. Uses a simple free list; ids start at 1.
    Entity createEntity() {
        if (!freeList_.empty()) {
            const Entity id = freeList_.back();
            freeList_.pop_back();
            return id;
        }
        return ++nextId_;
    }

    void destroyEntity(const Entity entity) {
        if (entity == kInvalidEntity || entity > nextId_) {
            return;
        }
        for (auto& entry : componentArrays_) {
            entry.second->erase(entity);
        }
        freeList_.push_back(entity);
    }

    [[nodiscard]] bool valid(const Entity entity) const noexcept {
        return entity != kInvalidEntity && entity <= nextId_
            && std::find(freeList_.begin(), freeList_.end(), entity)
                == freeList_.end();
    }

    template <typename T>
    ComponentArray<T>& componentArray() {
        const std::type_index key(typeid(T));
        auto iterator = componentArrays_.find(key);
        if (iterator == componentArrays_.end()) {
            auto owned = std::make_unique<ComponentArray<T>>();
            ComponentArray<T>* raw = owned.get();
            componentArrays_.emplace(key, std::move(owned));
            return *raw;
        }
        return *static_cast<ComponentArray<T>*>(iterator->second.get());
    }

    template <typename T>
    void addComponent(const Entity entity, T component) {
        componentArray<T>().insert(entity, std::move(component));
    }

    template <typename T>
    [[nodiscard]] T* tryGet(const Entity entity) noexcept {
        auto iterator = componentArrays_.find(std::type_index(typeid(T)));
        if (iterator == componentArrays_.end()) {
            return nullptr;
        }
        return static_cast<ComponentArray<T>*>(iterator->second.get())
            ->tryGet(entity);
    }

    template <typename T>
    [[nodiscard]] bool has(const Entity entity) const noexcept {
        const auto iterator = componentArrays_.find(std::type_index(typeid(T)));
        if (iterator == componentArrays_.end()) {
            return false;
        }
        return iterator->second->contains(entity);
    }

    // Iterate every entity that carries component T, invoking the callable
    // with (Entity, T&). The component may be mutated in place.
    template <typename T, typename Callable>
    void each(Callable&& callable) {
        ComponentArray<T>& array = componentArray<T>();
        for (auto& entry : array.components()) {
            callable(entry.first, entry.second);
        }
    }

    // Iterate entities carrying both T1 and T2.
    template <typename T1, typename T2, typename Callable>
    void each(Callable&& callable) {
        ComponentArray<T1>& primary = componentArray<T1>();
        ComponentArray<T2>& secondary = componentArray<T2>();
        for (auto& entry : primary.components()) {
            if (T2* second = secondary.tryGet(entry.first); second != nullptr) {
                callable(entry.first, entry.second, *second);
            }
        }
    }

    void addSystem(System system) {
        systems_.push_back(std::move(system));
    }

    void update() {
        for (System& system : systems_) {
            system(*this);
        }
    }

    [[nodiscard]] std::size_t entityCount() const noexcept {
        return static_cast<std::size_t>(nextId_) - freeList_.size();
    }

private:
    Entity nextId_ = 0;
    std::vector<Entity> freeList_;
    std::vector<System> systems_;
    std::unordered_map<std::type_index,
        std::unique_ptr<IComponentArray>> componentArrays_;
};

}  // namespace azurerender::ecs