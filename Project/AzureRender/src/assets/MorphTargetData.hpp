#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

struct MorphTargetData {
    std::string name;
    std::vector<std::array<float,3>> positions, normals, tangents;
};
struct MorphTargetRange {
    std::uint32_t node=0, firstVertex=0, vertexCount=0;
    std::vector<MorphTargetData> targets;
};
using MorphWeightState=std::vector<float>;
inline constexpr std::size_t kMorphTargetBudget=64;
