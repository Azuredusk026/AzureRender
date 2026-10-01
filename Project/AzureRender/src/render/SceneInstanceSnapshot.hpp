#pragma once

#include "scene/RenderBatching.hpp"
#include "render/RenderSettings.hpp"
#include <array>
#include <memory>
#include <utility>
#include <vector>

namespace azurerender {
struct RecordingGizmoState {
    std::int32_t selectedPrimitive = -1;
    bool active = false;
    std::array<float, 3> translation{};
    std::array<float, 3> rotation{};
    std::array<float, 3> scale{1, 1, 1};
};
struct RecordingBufferSet {
    std::vector<VkBuffer> vertices;
    std::vector<VkBuffer> indices;
    VkBuffer transparentIndices = VK_NULL_HANDLE;
    bool multiDrawIndirect = false;
    std::uint32_t maxDrawIndirectCount = 1;
    std::vector<std::vector<std::size_t>> transparentPrimitives;
};

// Published only as const storage. Visible entries use indices rather than
// pointers into the mutable frame preparation arrays.
struct SceneInstanceSnapshot final {
    const RenderSettings settings;
    const VkBuffer indirectBuffer;
    const std::uint32_t frameIndex;
    const RecordingGizmoState gizmo;
    const RecordingBufferSet buffers;
    const std::vector<scene::SceneInstance> instances;
    const std::vector<std::uint32_t> visibleIndices;
    const std::vector<std::array<std::uint32_t, 3>> opaqueSpans;
    const std::vector<std::array<std::uint32_t, 3>> visibleSpans;
    const std::vector<std::array<std::uint32_t, 3>> shadowSpans;
    const std::vector<std::size_t> indirectOffsets;
    const std::vector<std::size_t> transparentOffsets;

    SceneInstanceSnapshot(std::vector<scene::SceneInstance> source,
        std::vector<std::uint32_t> visible,
        std::vector<std::array<std::uint32_t, 3>> opaque,
        std::vector<std::array<std::uint32_t, 3>> shadow,
        std::vector<std::size_t> indirect, std::vector<std::size_t> transparent,
        std::vector<std::array<std::uint32_t, 3>> visibleBatches = {},
        RenderSettings renderSettings = {}, VkBuffer indirectHandle = VK_NULL_HANDLE,
        std::uint32_t frame = 0, RecordingGizmoState gizmoState = {},
        RecordingBufferSet bufferSet = {})
        : settings(std::move(renderSettings)), indirectBuffer(indirectHandle), frameIndex(frame),
          gizmo(std::move(gizmoState)), buffers(std::move(bufferSet)), instances(std::move(source)), visibleIndices(std::move(visible)),
          opaqueSpans(std::move(opaque)), visibleSpans(std::move(visibleBatches)), shadowSpans(std::move(shadow)),
          indirectOffsets(std::move(indirect)), transparentOffsets(std::move(transparent)) {}

    void validateForRecording() const {
        if (buffers.maxDrawIndirectCount == 0)
            throw std::logic_error("Snapshot indirect batch limit is zero");
        if (buffers.vertices.size() != buffers.indices.size())
            throw std::logic_error("Snapshot vertex/index resource counts differ");
        for (const auto index : visibleIndices)
            if (index >= instances.size()) throw std::out_of_range("Snapshot visible index");
        if (transparentOffsets.size() != instances.size())
            throw std::logic_error("Snapshot transparent offsets incomplete");
        if (indirectBuffer != VK_NULL_HANDLE && indirectOffsets.size() != instances.size())
            throw std::logic_error("Snapshot indirect offsets incomplete");
        for (std::size_t i = 0; i < instances.size(); ++i)
            if (instances[i].sourceIndex != i || instances[i].meshKey >= buffers.vertices.size())
                throw std::logic_error("Snapshot instance resource mapping invalid");
        const auto validateSpans = [this](const auto& spans) {
            for (const auto& span : spans) {
                if (span[1] > instances.size() || span[2] > instances.size() - span[1])
                    throw std::out_of_range("Snapshot draw span");
                for (std::size_t i = span[1]; i < static_cast<std::size_t>(span[1]) + span[2]; ++i)
                    if (instances[i].meshKey != span[0])
                        throw std::logic_error("Snapshot draw span mixes mesh resources");
            }
        };
        validateSpans(opaqueSpans);
        validateSpans(visibleSpans);
        validateSpans(shadowSpans);
    }
};

}  // namespace azurerender
