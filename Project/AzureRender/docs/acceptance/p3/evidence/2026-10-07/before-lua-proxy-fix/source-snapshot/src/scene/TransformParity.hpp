#pragma once
#include <array>

namespace azurerender::scene {
template <typename T>
constexpr T linearDeterminant(const std::array<T, 16>& m) noexcept {
    return m[0]*(m[5]*m[10]-m[9]*m[6])
         - m[4]*(m[1]*m[10]-m[9]*m[2])
         + m[8]*(m[1]*m[6]-m[5]*m[2]);
}
template <typename T>
constexpr bool mirroredTransform(const std::array<T, 16>& m) noexcept {
    return linearDeterminant(m) < T(0);
}
}
