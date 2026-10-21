#pragma once
#include "runtime/SceneDocument.hpp"
#include <optional>
#include <string>
#include <utility>

namespace azurerender {

// Tracks the resource snapshot acknowledged by a successful renderer load.
// The editor host owns this state and uses it only on its main thread.
class EditorRenderResourceState final {
public:
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
        append(std::to_string(document.resources.size()));
        for (const auto& resource : document.resources) {
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
