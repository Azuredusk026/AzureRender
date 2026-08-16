#pragma once

#include <cstdint>

namespace azurerender::ecs {

// A 32-bit identifier that is allocated by a World. Created entities start
// at 1 so that 0 can be reserved as the "invalid" sentinel.
using Entity = std::uint32_t;

constexpr Entity kInvalidEntity = 0;

}  // namespace azurerender::ecs