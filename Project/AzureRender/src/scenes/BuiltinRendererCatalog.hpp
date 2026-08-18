#pragma once

#include "extensions/ExtensionRegistry.hpp"

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
    [[nodiscard]] static const std::vector<ShaderFeatureDescriptor>&
    shaderFeatures();
};

}  // namespace azurerender
