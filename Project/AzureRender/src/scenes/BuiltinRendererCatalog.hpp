#pragma once

#include "extensions/ExtensionRegistry.hpp"
#include "render/RenderSettings.hpp"

#include <string>
#include <vector>

namespace azurerender {

struct ShaderFeatureDescriptor {
    std::string id;
    std::string rendererId;
    std::vector<std::string> shaderStages;
};

class BuiltinRendererCatalog final {
public:
    [[nodiscard]] static SceneRendererRegistry createRegistry();
    // Data-driven documents use the empty-scene backend until they own meshes.
    [[nodiscard]] static std::string documentRendererId(SceneType requested, bool hasResources);
    [[nodiscard]] static const std::vector<ShaderFeatureDescriptor>&
    shaderFeatures();
};

}  // namespace azurerender
