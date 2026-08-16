#pragma once

#include "IComponentArray.hpp"

#include <unordered_map>
#include <utility>

namespace azurerender::ecs {

// Per-type component storage. The component type id is derived from the
// template parameter so each T gets a unique array.
template <typename T>
class ComponentArray final : public IComponentArray {
public:
    void insert(Entity entity, T component) {
        components_[entity] = std::move(component);
    }
    [[nodiscard]] T* tryGet(Entity entity) noexcept {
        const auto iterator = components_.find(entity);
        return iterator == components_.end()
            ? nullptr
            : &iterator->second;
    }
    [[nodiscard]] const T* tryGet(Entity entity) const noexcept {
        const auto iterator = components_.find(entity);
        return iterator == components_.end()
            ? nullptr
            : &iterator->second;
    }
    [[nodiscard]] std::size_t size() const noexcept {
        return components_.size();
    }
    [[nodiscard]] std::unordered_map<Entity, T>& components() noexcept {
        return components_;
    }
    [[nodiscard]] const std::unordered_map<Entity, T>& components() const noexcept {
        return components_;
    }

    [[nodiscard]] std::size_t componentTypeId() const noexcept override {
        return TypeId<T>::value;
    }
    void erase(const Entity entity) noexcept override {
        components_.erase(entity);
    }
    [[nodiscard]] bool contains(const Entity entity) const noexcept override {
        return components_.find(entity) != components_.end();
    }

private:
    // Lightweight compile-time type id so each ComponentArray<T> can be
    // distinguished by a small integral without RTTI.
    template <typename>
    struct TypeId {
        static const std::size_t value;
    };

    std::unordered_map<Entity, T> components_;
};

template <typename T>
template <typename U>
const std::size_t ComponentArray<T>::TypeId<U>::value =
    reinterpret_cast<std::size_t>(&ComponentArray<T>::TypeId<U>::value);

}  // namespace azurerender::ecs