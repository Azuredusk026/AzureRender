#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace azurerender {
// Shared with shaders/instance_data.glsl. Writers retire the owning frame fence.
struct alignas(16) SceneInstanceGpuData {
    static constexpr unsigned kLayoutVersion=1;
    std::array<float,16> model{};
    std::array<float,16> modelViewProjection{};
    std::array<std::array<float,16>,4> cascadeLightModelViewProjection{};
    std::array<std::uint32_t,4> meta{};
    std::array<float,4> faceLight{};
    std::array<float,4> morph{};
    std::array<float,16> faceFrame{};
};
static_assert(std::is_standard_layout_v<SceneInstanceGpuData>);
static_assert(std::is_trivially_copyable_v<SceneInstanceGpuData>);
static_assert(sizeof(SceneInstanceGpuData)==496);
static_assert(alignof(SceneInstanceGpuData)==16);
static_assert(offsetof(SceneInstanceGpuData,model)==0);
static_assert(offsetof(SceneInstanceGpuData,modelViewProjection)==64);
static_assert(offsetof(SceneInstanceGpuData,cascadeLightModelViewProjection)==128);
static_assert(offsetof(SceneInstanceGpuData,meta)==384);
static_assert(offsetof(SceneInstanceGpuData,faceLight)==400);
static_assert(offsetof(SceneInstanceGpuData,morph)==416);
static_assert(offsetof(SceneInstanceGpuData,faceFrame)==432);
}
