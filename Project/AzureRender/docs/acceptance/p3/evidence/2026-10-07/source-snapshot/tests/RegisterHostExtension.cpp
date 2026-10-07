#include "runtime/ComponentRegistry.hpp"

namespace {
struct AssetNote {
    std::string text = "untitled";
    std::uint32_t sampleCount = 3;
};

const bool registered = [] {
    using namespace azurerender;
    auto type = reflection::reflectedType<AssetNote>("tool.asset-note", 1, {
        reflection::property<AssetNote>("text", "Text", &AssetNote::text, 0, 0),
        reflection::property<AssetNote>("sampleCount", "Samples", &AssetNote::sampleCount, 0, 100)});
    type.properties[0].category = "Inspection";
    type.properties[0].tooltip = "Project-owned inspection note";
    type.properties[1].readOnly = true;
    runtimeComponentRegistry().registerComponent<AssetNote>(std::move(type));
    return true;
}();
}
