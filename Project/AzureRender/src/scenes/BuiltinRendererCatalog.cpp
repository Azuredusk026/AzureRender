#include "BuiltinRendererCatalog.hpp"

#include "BlackholeSceneRenderer.hpp"
#include "CharacterSceneRenderer.hpp"
#include "SampleSceneRenderer.hpp"
#include "pathtracing/PathTracingSceneRenderer.hpp"

#include <memory>

namespace azurerender {

std::string BuiltinRendererCatalog::documentRendererId(SceneType requested, bool hasResources) {
    if (requested == SceneType::Character || requested == SceneType::Sample)
        return hasResources ? "character" : "sample";
    return sceneTypeName(requested);
}

SceneRendererRegistry BuiltinRendererCatalog::createRegistry() {
    SceneRendererRegistry registry;
    registry.registerFactory({"pathtracing",SceneRendererRegistry::kApiVersion,{"geometry","progressive","capture"},{}},[]{return std::make_unique<PathTracingSceneRenderer>();});
    registry.registerFactory(
        {"character", SceneRendererRegistry::kApiVersion, {"geometry", "editor", "capture"}, {}},
        [] { return std::make_unique<CharacterSceneRenderer>(); });
    registry.registerFactory(
        {"blackhole", SceneRendererRegistry::kApiVersion, {"fullscreen", "temporal", "capture"}, {}},
        [] { return std::make_unique<BlackholeSceneRenderer>(); });
    registry.registerFactory(
        {"sample", SceneRendererRegistry::kApiVersion, {"sdk-example", "capture"}, {}},
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
        {"pathtracing.integrator", "pathtracing", {"pathtracing.comp", "pathtracing.frag"}},
        {"sample.clear", "sample", {}},
    };
    return features;
}

}  // namespace azurerender
