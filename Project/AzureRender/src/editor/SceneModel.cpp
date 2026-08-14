#include "SceneModel.hpp"

#include <fstream>
#include <iomanip>
#include <stdexcept>

namespace azurerender {

namespace {

void requireHeader(std::istream& input, const std::string& expected) {
    std::string header;
    std::getline(input, header);
    if (header != expected) {
        throw std::runtime_error(
            "Invalid .azscene header; expected " + expected);
    }
}

bool readBool(std::istream& input, const char* name) {
    std::string key;
    bool value = false;
    if (!(input >> key >> std::boolalpha >> value) || key != name) {
        throw std::runtime_error(std::string("Missing .azscene field: ") + name);
    }
    return value;
}

}  // namespace

SceneDocument SceneDocument::fromAsset(
    const std::filesystem::path& assetPath) {
    if (assetPath.empty()) {
        throw std::invalid_argument("Scene asset path must not be empty");
    }
    SceneDocument document;
    document.sceneId = assetPath.stem().string();
    document.resources.push_back({"asset-0", "gltf", assetPath});
    document.nodes.push_back({"root", document.sceneId, "", "asset-0", true});
    return document;
}

SceneDocument SceneDocument::load(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("Unable to open scene: " + path.string());
    }
    requireHeader(input, "AzureRender Scene v1");
    SceneDocument document;
    std::uint32_t schemaVersion = 0;
    std::size_t resourceCount = 0;
    std::size_t nodeCount = 0;
    std::string key;
    if (!(input >> key >> schemaVersion) || key != "schemaVersion"
        || schemaVersion != kSchemaVersion) {
        throw std::runtime_error("Unsupported .azscene schemaVersion");
    }
    if (!(input >> key >> std::quoted(document.sceneId)) || key != "sceneId") {
        throw std::runtime_error("Missing .azscene sceneId");
    }
    if (!(input >> key >> resourceCount) || key != "resourceCount") {
        throw std::runtime_error("Missing .azscene resourceCount");
    }
    for (std::size_t index = 0; index < resourceCount; ++index) {
        SceneResource resource;
        std::string resourcePath;
        if (!(input >> key >> std::quoted(resource.id)
              >> std::quoted(resource.type) >> std::quoted(resourcePath))
            || key != "resource") {
            throw std::runtime_error("Invalid .azscene resource");
        }
        resource.path = resourcePath;
        document.resources.push_back(std::move(resource));
    }
    if (!(input >> key >> nodeCount) || key != "nodeCount") {
        throw std::runtime_error("Missing .azscene nodeCount");
    }
    for (std::size_t index = 0; index < nodeCount; ++index) {
        SceneNode node;
        if (!(input >> key >> std::quoted(node.id)
              >> std::quoted(node.name) >> std::quoted(node.parentId)
              >> std::quoted(node.resourceId)) || key != "node") {
            throw std::runtime_error("Invalid .azscene node");
        }
        node.visible = readBool(input, "visible");
        document.nodes.push_back(std::move(node));
    }
    if (!(input >> key >> document.renderSettings.showcasePreset)
        || key != "showcasePreset") {
        throw std::runtime_error("Missing .azscene showcasePreset");
    }
    if (!(input >> key >> document.renderSettings.styleMaskStrength)
        || key != "styleMaskStrength") {
        throw std::runtime_error("Missing .azscene styleMaskStrength");
    }
    if (!(input >> key >> document.renderSettings.diffuseBandThreshold)
        || key != "diffuseBandThreshold") {
        throw std::runtime_error("Missing .azscene diffuseBandThreshold");
    }
    if (!(input >> key >> std::boolalpha
          >> document.renderSettings.innerOutlineEnabled)
        || key != "innerOutlineEnabled") {
        throw std::runtime_error("Missing .azscene innerOutlineEnabled");
    }
    if (!(input >> key >> document.renderSettings.outline.strength)
        || key != "outlineStrength") {
        throw std::runtime_error("Missing .azscene outlineStrength");
    }
    if (!(input >> key >> document.renderSettings.grade.exposureEv)
        || key != "gradeExposureEv") {
        throw std::runtime_error("Missing .azscene gradeExposureEv");
    }
    validateRenderSettings(document.renderSettings);
    return document;
}

void SceneDocument::save(const std::filesystem::path& path) const {
    validateRenderSettings(renderSettings);
    std::ofstream output(path);
    if (!output) {
        throw std::runtime_error("Unable to write scene: " + path.string());
    }
    output << "AzureRender Scene v1\n"
           << "schemaVersion " << kSchemaVersion << '\n'
           << "sceneId " << std::quoted(sceneId) << '\n'
           << "resourceCount " << resources.size() << '\n';
    for (const SceneResource& resource : resources) {
        output << "resource " << std::quoted(resource.id) << ' '
               << std::quoted(resource.type) << ' '
               << std::quoted(resource.path.string()) << '\n';
    }
    output << "nodeCount " << nodes.size() << '\n';
    for (const SceneNode& node : nodes) {
        output << "node " << std::quoted(node.id) << ' '
               << std::quoted(node.name) << ' '
               << std::quoted(node.parentId) << ' '
               << std::quoted(node.resourceId) << '\n'
               << "visible " << std::boolalpha << node.visible << '\n';
    }
    output << "showcasePreset " << renderSettings.showcasePreset << '\n'
           << "styleMaskStrength " << renderSettings.styleMaskStrength << '\n'
           << "diffuseBandThreshold " << renderSettings.diffuseBandThreshold << '\n'
           << "innerOutlineEnabled " << std::boolalpha
           << renderSettings.innerOutlineEnabled << '\n'
           << "outlineStrength " << renderSettings.outline.strength << '\n'
           << "gradeExposureEv " << renderSettings.grade.exposureEv << '\n';
}

}  // namespace azurerender
