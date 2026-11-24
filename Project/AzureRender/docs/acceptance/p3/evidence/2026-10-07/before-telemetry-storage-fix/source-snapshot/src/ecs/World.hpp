#pragma once

#include "ComponentArray.hpp"
#include "Entity.hpp"
#include "IComponentArray.hpp"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <stdexcept>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace azurerender::ecs {

struct EntityHandle {
    Entity entity = kInvalidEntity;
    std::uint64_t generation = 0;
};

// A system is just a callable invoked once per World::update.
using System = std::function<void(class World&)>;

// Owned component arrays indexed by std::type_index for fast lookup.
class World final {
public:
    World() = default;
    World(const World&) = delete;
    World& operator=(const World&) = delete;

    void swap(World& other) noexcept {
        std::swap(nextId_, other.nextId_);
        freeList_.swap(other.freeList_);
        identities_.swap(other.identities_);
        systems_.swap(other.systems_);
        componentArrays_.swap(other.componentArrays_);
    }

    void clear() noexcept {
        systems_.clear();componentArrays_.clear();freeList_.clear();identities_.clear();nextId_=0;
    }

    // Allocate a new entity. Uses a simple free list; ids start at 1.
    Entity createEntity() {
        const auto identity=nextIdentity();
        if (!freeList_.empty()) {
            const Entity id = freeList_.back();
            freeList_.pop_back();
            identities_[id]=identity;
            return id;
        }
        if(nextId_==std::numeric_limits<Entity>::max())throw std::length_error("Entity capacity exhausted");
        identities_.resize(static_cast<std::size_t>(nextId_)+2);
        identities_[nextId_+1]=identity;
        return ++nextId_;
    }

    void destroyEntity(const Entity entity) {
        if (!valid(entity)) {
            return;
        }
        for (auto& entry : componentArrays_) {
            entry.second->erase(entity);
        }
        freeList_.push_back(entity);
        identities_[entity]=0;
    }

    [[nodiscard]] bool valid(const Entity entity) const noexcept {
        return entity != kInvalidEntity && entity < identities_.size() && identities_[entity]!=0;
    }

    [[nodiscard]] EntityHandle handle(Entity entity) const {
        if(!valid(entity))throw std::invalid_argument("Cannot capture an invalid entity");
        return {entity,identities_[entity]};
    }
    [[nodiscard]] bool valid(EntityHandle handle) const noexcept {
        return valid(handle.entity) && identities_[handle.entity]==handle.generation;
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
    void removeComponent(const Entity entity) noexcept {
        const auto iterator = componentArrays_.find(std::type_index(typeid(T)));
        if (iterator != componentArrays_.end()) {
            iterator->second->erase(entity);
        }
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
    [[nodiscard]] const T* tryGet(const Entity entity) const noexcept {
        const auto iterator = componentArrays_.find(std::type_index(typeid(T)));
        if (iterator == componentArrays_.end()) return nullptr;
        return static_cast<const ComponentArray<T>*>(iterator->second.get())->tryGet(entity);
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
        for (std::size_t index = 0; index < array.dense().size(); ++index) {
            callable(array.entities()[index], array.dense()[index]);
        }
    }

    // Iterate entities carrying both T1 and T2.
    template <typename T1, typename T2, typename Callable>
    void each(Callable&& callable) {
        ComponentArray<T1>& primary = componentArray<T1>();
        ComponentArray<T2>& secondary = componentArray<T2>();
        for (std::size_t index = 0; index < primary.dense().size(); ++index) {
            const Entity entity = primary.entities()[index];
            if (T2* second = secondary.tryGet(entity); second != nullptr) {
                callable(entity, primary.dense()[index], *second);
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
    static std::uint64_t nextIdentity() {
        // Process-wide incarnations prevent cross-world and clear/rebuild ABA.
        static std::atomic<std::uint64_t> next{0};
        auto current=next.load(std::memory_order_relaxed);
        do {
            if(current==std::numeric_limits<std::uint64_t>::max())throw std::length_error("Entity identity exhausted");
        }while(!next.compare_exchange_weak(current,current+1,std::memory_order_relaxed));
        return current+1;
    }
    Entity nextId_ = 0;
    std::vector<std::uint64_t> identities_;
    std::vector<Entity> freeList_;
    std::vector<System> systems_;
    std::unordered_map<std::type_index,
        std::unique_ptr<IComponentArray>> componentArrays_;
};

}  // namespace azurerender::ecs
