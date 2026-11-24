#pragma once
#include <array>

namespace azurerender {
// GLFW key values shared by the physical keyboard and deterministic replay.
inline constexpr bool isGameplayKey(int key) {
    return key==32 || key==39 || (key>=44&&key<=57) || key==59 || key==61
        || (key>=65&&key<=93) || key==96 || key==161 || key==162
        || (key>=256&&key<=269) || (key>=280&&key<=284) || (key>=290&&key<=314)
        || (key>=320&&key<=336) || (key>=340&&key<=348);
}
}
