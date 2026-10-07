#pragma once
#include "GltfLoader.hpp"
#include <algorithm>
#include <cctype>
#include <map>
#include <set>

namespace azurerender {
// Prepare named islands before skinning; eye and eyelash geometry is preserved.
inline std::size_t prepareBrowRegions(LoadedAsset& asset) {
    std::vector<bool> browJoints(asset.jointNodes.size(), false);
    for (std::size_t i = 0; i < asset.jointNodes.size(); ++i) {
        auto name = asset.nodes.at(asset.jointNodes[i]).name;
        std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        browJoints[i] = name.find("brow") != std::string::npos;
    }
    std::size_t islands = 0;
    for (const auto& primitive : asset.primitives) {
        const auto& material = asset.materials.at(primitive.materialIndex);
        if (!(material.materialFeatures & MaterialFeatureBrowOverlay)) continue;
        const float thickness = std::clamp(material.styleParameters[0], 0.0F, 0.0012F);
        std::map<std::uint32_t, std::vector<std::uint32_t>> neighbors;
        auto selected = [&](std::uint32_t index) {
            const auto& v = asset.vertices.at(index);
            for (std::size_t i = 0; i < 4; ++i)
                if (v.weights[i] > 0.001F && v.joints[i] < browJoints.size() && browJoints[v.joints[i]]) return true;
            return false;
        };
        for (std::uint32_t offset = 0; offset < primitive.indexCount; offset += 3) {
            for (std::uint32_t i = 0; i < 3; ++i) {
                auto a = asset.indices.at(primitive.firstIndex + offset + i);
                auto b = asset.indices.at(primitive.firstIndex + offset + (i + 1) % 3);
                if (selected(a)) neighbors[a];
                if (selected(a) && selected(b)) { neighbors[a].push_back(b); neighbors[b].push_back(a); }
            }
        }
        std::set<std::uint32_t> visited;
        for (const auto& entry : neighbors) {
            if (visited.count(entry.first)) continue;
            std::vector<std::uint32_t> region, queue{entry.first};
            visited.insert(entry.first);
            std::array<float, 2> low{1e30F, 1e30F}, high{-1e30F, -1e30F};
            while (!queue.empty()) {
                auto index = queue.back(); queue.pop_back(); region.push_back(index);
                const auto& position = asset.vertices[index].position;
                for (unsigned axis = 0; axis < 2; ++axis) {
                    low[axis] = std::min(low[axis], position[axis]); high[axis] = std::max(high[axis], position[axis]);
                }
                for (auto next : neighbors.at(index)) if (visited.insert(next).second) queue.push_back(next);
            }
            ++islands;
            for (auto index : region) {
                asset.vertices[index].browMask = 1.0F;
                auto& position = asset.vertices[index].position;
                for (unsigned axis = 0; axis < 2; ++axis) {
                    const auto delta = position[axis] - (low[axis] + high[axis]) * 0.5F;
                    position[axis] += (delta > 0 ? 1 : delta < 0 ? -1 : 0) * thickness * (axis == 0 ? 0.45F : 1.0F);
                }
            }
        }
    }
    return islands;
}
} // namespace azurerender
