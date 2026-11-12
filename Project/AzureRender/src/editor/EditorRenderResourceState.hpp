#pragma once
#include "runtime/SceneDocument.hpp"
#include "scene/SceneDescription.hpp"
#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace azurerender {

// Tracks the resource snapshot acknowledged by a successful renderer load.
// The editor host owns this state and uses it only on its main thread.
class EditorRenderResourceState final {
public:
    template<class Node>
    [[nodiscard]] static std::unordered_set<std::string_view> referencedResources(const std::vector<Node>& nodes) {
        std::unordered_set<std::string_view> result;
        result.reserve(nodes.size());
        for (const auto& node : nodes) {
            if (!node.resourceId.empty()) result.emplace(node.resourceId);
        }
        return result;
    }

    [[nodiscard]] static scene::SceneDescription describe(const SceneDocument& document) {
        auto description = document.renderDescription();
        const auto referenced = referencedResources(description.nodes);
        description.resources.erase(std::remove_if(description.resources.begin(), description.resources.end(),
            [&](const auto& resource) { return referenced.count(resource.id) == 0; }), description.resources.end());
        return description;
    }

    [[nodiscard]] bool needsReload(const SceneDocument& document) const {
        return !loaded_ || *loaded_ != signature(document);
    }

    template<class Loader>
    void load(const SceneDocument& document, Loader&& loader) {
        auto submitted = signature(document);
        std::forward<Loader>(loader)();
        loaded_ = std::move(submitted);
    }

private:
    std::optional<std::string> loaded_;

    static std::string signature(const SceneDocument& document) {
        std::string result;
        const auto append = [&](const std::string& field) {
            result += std::to_string(field.size()) + ":" + field;
        };
        append(sceneTypeName(document.renderSettings.sceneType));
        const auto referenced = referencedResources(document.nodes);
        const auto isReferenced = [&](const SceneResource& resource) { return referenced.count(resource.id) != 0; };
        append(std::to_string(std::count_if(document.resources.begin(), document.resources.end(), isReferenced)));
        for (const auto& resource : document.resources) {
            if (!isReferenced(resource)) continue;
            append(resource.id);
            append(resource.path.generic_u8string());
        }
        append(std::to_string(document.nodes.size()));
        for (const auto& node : document.nodes) {
            append(node.id);
            append(node.resourceId);
            append(std::to_string(node.resolvedMaterials.size()));
            for (const auto& [slot, binding] : node.resolvedMaterials) {
                append(slot);
                append(binding.path);
                append(binding.fingerprint);
            }
        }
        return result;
    }
};

} // namespace azurerender
