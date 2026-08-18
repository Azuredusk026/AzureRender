#include "BuiltinRendererCatalog.hpp"

#include "BlackholeSceneRenderer.hpp"
#include "CharacterSceneRenderer.hpp"
#include "SampleSceneRenderer.hpp"

#include <memory>

namespace azurerender {

SceneRendererRegistry BuiltinRendererCatalog::createRegistry() {
    SceneRendererRegistry registry;
    registry.registerFactory(
        {"character", 1, {"geometry", "editor", "capture"}, {}},
        [] { return std::make_unique<CharacterSceneRenderer>(); });
    registry.registerFactory(
        {"blackhole", 1, {"fullscreen", "temporal", "capture"}, {}},
        [] { return std::make_unique<BlackholeSceneRenderer>(); });
    registry.registerFactory(
        {"sample", 1, {"sdk-example", "capture"}, {}},
        [] { return std::make_unique<SampleSceneRenderer>(); });
    return registry;
}

const std::vector<ShaderFeatureDescriptor>&
BuiltinRendererCatalog::shaderFeatures() {
    static const std::vector<ShaderFeatureDescriptor> features{
        {"character.material", "character", {"mesh.vert", "mesh.frag"}},
        {"character.outline", "character", {"outline.vert", "outline.frag"}},
        {"blackhole.trace", "blackhole", {"blackhole.vert", "blackhole.frag"}},
        {"blackhole.temporal", "blackhole", {"blackhole_taa.frag", "blackhole_composite.frag"}},
        {"sample.clear", "sample", {}},
    };
    return features;
}

}  // namespace azurerender
