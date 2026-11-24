#pragma once

#include "IComponentArray.hpp"

#include <cstdint>
#include <utility>
#include <vector>

namespace azurerender::ecs {

// Per-type component storage as a sparse set: components live in one dense
// vector with a parallel entity vector, and a sparse index maps entity id to
// dense slot (0 = absent). Iteration is contiguous and cache-friendly, which
// is what makes render data traversal and later multi-threaded chunking
// cheap. Erase swaps the last element into the freed slot, so dense order is
// insertion order except across an erase.
template <typename T>
class ComponentArray final : public IComponentArray {
public:
    void insert(const Entity entity, T component) {
        if (entity >= sparse_.size()) {
            sparse_.resize(entity + 1, 0);
        }
        const std::uint32_t slot = sparse_[entity];
        if (slot != 0) {
            dense_[slot - 1] = std::move(component);
            return;
        }
        sparse_[entity] = static_cast<std::uint32_t>(dense_.size()) + 1;
        dense_.push_back(std::move(component));
        denseEntities_.push_back(entity);
    }

    [[nodiscard]] T* tryGet(const Entity entity) noexcept {
        if (entity >= sparse_.size()) {
            return nullptr;
        }
        const std::uint32_t slot = sparse_[entity];
        return slot == 0 ? nullptr : &dense_[slot - 1];
    }
    [[nodiscard]] const T* tryGet(const Entity entity) const noexcept {
        if (entity >= sparse_.size()) {
            return nullptr;
        }
        const std::uint32_t slot = sparse_[entity];
        return slot == 0 ? nullptr : &dense_[slot - 1];
    }

    [[nodiscard]] std::size_t size() const noexcept { return dense_.size(); }

    // Dense views for contiguous traversal. The entity at index i owns the
    // component at index i.
    [[nodiscard]] std::vector<T>& dense() noexcept { return dense_; }
    [[nodiscard]] const std::vector<T>& dense() const noexcept {
        return dense_;
    }
    [[nodiscard]] const std::vector<Entity>& entities() const noexcept {
        return denseEntities_;
    }

    [[nodiscard]] std::size_t componentTypeId() const noexcept override {
        return TypeId<T>::value;
    }

    void erase(const Entity entity) noexcept override {
        if (entity >= sparse_.size()) {
            return;
        }
        const std::uint32_t slot = sparse_[entity];
        if (slot == 0) {
            return;
        }
        const std::size_t index = slot - 1;
        const std::size_t last = dense_.size() - 1;
        if (index != last) {
            dense_[index] = std::move(dense_[last]);
            denseEntities_[index] = denseEntities_[last];
            sparse_[denseEntities_[index]] = slot;
        }
        dense_.pop_back();
        denseEntities_.pop_back();
        sparse_[entity] = 0;
    }

    [[nodiscard]] bool contains(const Entity entity) const noexcept override {
        return entity < sparse_.size() && sparse_[entity] != 0;
    }

private:
    // Lightweight compile-time type id so each ComponentArray<T> can be
    // distinguished by a small integral without RTTI.
    template <typename>
    struct TypeId {
        static const std::size_t value;
    };

    std::vector<T> dense_;
    std::vector<Entity> denseEntities_;
    std::vector<std::uint32_t> sparse_;
};

template <typename T>
template <typename U>
const std::size_t ComponentArray<T>::TypeId<U>::value =
    reinterpret_cast<std::size_t>(&ComponentArray<T>::TypeId<U>::value);

}  // namespace azurerender::ecs
