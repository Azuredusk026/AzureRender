#pragma once
#include <array>

namespace azurerender {
// GLFW key values shared by the physical keyboard and deterministic replay.
inline constexpr std::array<int,9> kGameplayKeys{32,65,68,69,82,83,87,340,344};
inline constexpr bool isGameplayKey(int key) {
    for(const auto allowed:kGameplayKeys)if(key==allowed)return true;
    return false;
}
}
