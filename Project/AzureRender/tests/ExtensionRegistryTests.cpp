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
    return 0;
}
