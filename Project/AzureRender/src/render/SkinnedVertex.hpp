#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
namespace azurerender {
struct GpuSkinnedVertex {
    std::array<float,3> position{},normal{};
    std::array<float,4> tangent{};
    static constexpr std::uint32_t wordCount=10;
};
static_assert(sizeof(GpuSkinnedVertex)==40&&offsetof(GpuSkinnedVertex,normal)==12&&offsetof(GpuSkinnedVertex,tangent)==24);
}
