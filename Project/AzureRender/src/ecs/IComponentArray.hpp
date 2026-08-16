#pragma once

#include "Entity.hpp"

#include <cstddef>

namespace azurerender::ecs {

// Type-erased interface for per-component-type dense storage. Implementations
// are produced by the World via the component_array<T>() accessor and are
// owned by the World.
class IComponentArray {
public:
    virtual ~IComponentArray() = default;
    [[nodiscard]] virtual std::size_t componentTypeId() const noexcept = 0;
    virtual void erase(Entity entity) noexcept = 0;
    [[nodiscard]] virtual bool contains(Entity entity) const noexcept = 0;
};

}  // namespace azurerender::ecs