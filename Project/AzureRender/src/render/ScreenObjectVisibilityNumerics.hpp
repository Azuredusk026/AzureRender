#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace azurerender {

enum class ScreenObjectVisibilityArithmetic {Device,ProfileV1};

inline constexpr std::size_t kScreenObjectVisibilityProfileWords=1u<<19;
inline constexpr std::size_t kScreenObjectVisibilityProfileBytes=kScreenObjectVisibilityProfileWords*sizeof(std::uint32_t);

inline constexpr bool validScreenObjectVisibilityProfileWord(std::uint32_t word) noexcept {
    return (word&(word>>1)&0x55555555u)==0;
}

struct ScreenObjectVisibilityNumerics {
    ScreenObjectVisibilityArithmetic mode=ScreenObjectVisibilityArithmetic::Device;
    // ProfileV1: 2^23 normalized mantissas, sixteen two-bit corrections per word.
    // Codes 0/1/2 mean -1/0/+1 ULP from nearest-even reciprocal; 3 is reserved.
    std::vector<std::uint32_t> reciprocalCorrections;
};

} // namespace azurerender
