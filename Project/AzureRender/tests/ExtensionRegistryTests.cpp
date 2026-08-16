#include "extensions/ExtensionRegistry.hpp"

#include <cassert>
#include <memory>
#include <stdexcept>
#include <string_view>

namespace {

class TestFeature final : public azurerender::IRenderFeature {
public:
    [[nodiscard]] std::string_view name() const noexcept override {
        return "test";
    }
};

class TestSceneRenderer final : public azurerender::ISceneRenderer {
public:
    [[nodiscard]] std::string_view name() const noexcept override {
        return "test-scene";
    }
    [[nodiscard]] azurerender::SceneRendererCapabilities capabilities()
        const override {
        azurerender::SceneRendererCapabilities caps;
        caps.diagnosticViewNames = {"Beauty", "Photon Ring"};
        return caps;
    }
    void onLoad(const azurerender::RenderContext&) override {}
    void onSwapchainRecreate(const azurerender::RenderContext&) override {}
    void updateFrame(const azurerender::SceneFrameData&) override {}
    void recordScene(const azurerender::RenderContext&) override {}
    void onUnload(const azurerender::RenderContext&) override {}
};

}  // namespace

int main() {
    azurerender::RenderFeatureRegistry registry;
    registry.registerFactory(
        {"feature.base", 1, {"render"}, {}},
        [] { return std::make_unique<TestFeature>(); });
    registry.registerFactory(
        {"feature.child", 1, {}, {"feature.base"}},
        [] { return std::make_unique<TestFeature>(); });
    assert(registry.contains("feature.base"));
    assert(registry.createAll().size() == 2);

    bool duplicateFailed = false;
    try {
        registry.registerFactory(
            {"feature.base", 1, {}, {}},
            [] { return std::make_unique<TestFeature>(); });
    } catch (const std::runtime_error&) {
        duplicateFailed = true;
    }
    assert(duplicateFailed);

    bool versionFailed = false;
    try {
        registry.registerFactory(
            {"feature.future", 2, {}, {}},
            [] { return std::make_unique<TestFeature>(); });
    } catch (const std::runtime_error&) {
        versionFailed = true;
    }
    assert(versionFailed);

    bool dependencyFailed = false;
    try {
        azurerender::RenderFeatureRegistry empty;
        empty.registerFactory(
            {"feature.orphan", 1, {}, {"feature.missing"}},
            [] { return std::make_unique<TestFeature>(); });
    } catch (const std::runtime_error&) {
        dependencyFailed = true;
    }
    assert(dependencyFailed);

    // Scene renderer registry reuses the same dependency/version contract and
    // the default diagnostic view lookup comes from capabilities.
    azurerender::SceneRendererRegistry sceneRegistry;
    sceneRegistry.registerFactory(
        {"scene.character", 1, {"render"}, {}},
        [] { return std::make_unique<TestSceneRenderer>(); });
    assert(sceneRegistry.contains("scene.character"));
    const auto scenes = sceneRegistry.createAll();
    assert(scenes.size() == 1);
    assert(scenes[0]->name() == "test-scene");
    assert(scenes[0]->diagnosticViewName(0) == "Beauty");
    assert(scenes[0]->diagnosticViewName(1) == "Photon Ring");
    assert(scenes[0]->diagnosticViewName(99) == "Unknown");
    return 0;
}
