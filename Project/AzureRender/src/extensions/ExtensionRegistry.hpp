#pragma once

#include "IAssetImporter.hpp"
#include "IRenderFeature.hpp"
#include "ISceneRenderer.hpp"
#include "editor/IEditorPanel.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace azurerender {

struct ExtensionDescriptor {
    std::string id;
    std::uint32_t apiVersion = 1;
    std::vector<std::string> capabilities;
    std::vector<std::string> dependencies;
};

template <typename Interface>
class ExtensionRegistry final {
public:
    using Factory = std::function<std::unique_ptr<Interface>()>;

    void registerFactory(ExtensionDescriptor descriptor, Factory factory) {
        if (descriptor.id.empty()) {
            throw std::invalid_argument("Extension ID cannot be empty");
        }
        if (descriptor.apiVersion != kApiVersion) {
            throw std::runtime_error(
                "Extension '" + descriptor.id + "' requires incompatible API version "
                + std::to_string(descriptor.apiVersion));
        }
        if (!factory) {
            throw std::invalid_argument(
                "Extension '" + descriptor.id + "' has no factory");
        }
        if (ids_.find(descriptor.id) != ids_.end()) {
            throw std::runtime_error("Duplicate extension ID: " + descriptor.id);
        }
        for (const auto& dependency : descriptor.dependencies) {
            if (ids_.find(dependency) == ids_.end()) {
                throw std::runtime_error(
                    "Extension '" + descriptor.id
                    + "' has missing dependency: " + dependency);
            }
        }
        ids_.insert(descriptor.id);
        entries_.push_back({std::move(descriptor), std::move(factory)});
    }

    [[nodiscard]] bool contains(const std::string& id) const noexcept {
        return ids_.find(id) != ids_.end();
    }
    [[nodiscard]] std::vector<std::unique_ptr<Interface>> createAll() const {
        std::vector<std::unique_ptr<Interface>> instances;
        instances.reserve(entries_.size());
        for (const auto& entry : entries_) {
            auto instance = entry.factory();
            if (instance == nullptr) {
                throw std::runtime_error(
                    "Extension factory returned null: " + entry.descriptor.id);
            }
            instances.push_back(std::move(instance));
        }
        return instances;
    }
    [[nodiscard]] const std::vector<ExtensionDescriptor> descriptors() const {
        std::vector<ExtensionDescriptor> result;
        result.reserve(entries_.size());
        for (const auto& entry : entries_) result.push_back(entry.descriptor);
        return result;
    }

    static constexpr std::uint32_t kApiVersion = 1;

private:
    struct Entry {
        ExtensionDescriptor descriptor;
        Factory factory;
    };
    std::vector<Entry> entries_;
    std::unordered_set<std::string> ids_;
};

using RenderFeatureRegistry = ExtensionRegistry<IRenderFeature>;
using AssetImporterRegistry = ExtensionRegistry<IAssetImporter>;
using EditorPanelRegistry = ExtensionRegistry<IEditorPanel>;
using SceneRendererRegistry = ExtensionRegistry<ISceneRenderer>;

}  // namespace azurerender
