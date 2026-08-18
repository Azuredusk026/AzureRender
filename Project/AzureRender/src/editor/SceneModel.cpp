#include "SceneModel.hpp"

#include <chrono>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <random>
#include <stdexcept>
#include <system_error>

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

// Writes the document payload through the supplied stream. Shared by the
// normal save path and the atomic-save path so both produce identical bytes.
void writeDocument(std::ostream& output, const SceneDocument& document) {
    output << "AzureRender Scene v1\n"
           << "schemaVersion " << SceneDocument::kSchemaVersion << '\n'
           << "renderSettingsVersion " << RenderSettings::kSchemaVersion << '\n'
           << "sceneId " << std::quoted(document.sceneId) << '\n'
           << "resourceCount " << document.resources.size() << '\n';
    for (const SceneResource& resource : document.resources) {
        output << "resource " << std::quoted(resource.id) << ' '
               << std::quoted(resource.type) << ' '
               << std::quoted(resource.path.string()) << '\n';
    }
    output << "nodeCount " << document.nodes.size() << '\n';
    for (const SceneNode& node : document.nodes) {
        output << "node " << std::quoted(node.id) << ' '
               << std::quoted(node.name) << ' '
               << std::quoted(node.parentId) << ' '
               << std::quoted(node.resourceId) << '\n'
               << "visible " << std::boolalpha << node.visible << '\n'
               << "transform "
               << node.translation[0] << ' ' << node.translation[1] << ' '
               << node.translation[2] << ' ' << node.rotation[0] << ' '
               << node.rotation[1] << ' ' << node.rotation[2] << ' '
               << node.scale[0] << ' ' << node.scale[1] << ' '
               << node.scale[2] << '\n'
               << "prefab " << std::quoted(node.prefabSource) << ' '
               << std::quoted(node.instanceOf) << '\n';
    }
    output << "showcasePreset " << document.renderSettings.showcasePreset << '\n'
           << "styleMaskStrength " << document.renderSettings.styleMaskStrength << '\n'
           << "diffuseBandThreshold " << document.renderSettings.diffuseBandThreshold << '\n'
           << "innerOutlineEnabled " << std::boolalpha
           << document.renderSettings.innerOutlineEnabled << '\n'
           << "outlineStrength " << document.renderSettings.outline.strength << '\n'
           << "gradeExposureEv " << document.renderSettings.grade.exposureEv << '\n'
           << "characterBackgroundEnabled " << std::boolalpha
           << document.renderSettings.characterPresentation.backgroundEnabled << '\n'
           << "characterPlatformEnabled " << std::boolalpha
           << document.renderSettings.characterPresentation.platformEnabled << '\n'
           << "blackholeQuality "
           << blackholeQualityName(document.renderSettings.blackhole.quality) << '\n'
           << "blackholeCamera "
           << blackholeCameraName(document.renderSettings.blackhole.camera) << '\n'
           << "sceneRenderer "
           << sceneTypeName(document.renderSettings.sceneType) << '\n';
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
    SceneNode root;
    root.id = "root";
    root.name = document.sceneId;
    root.resourceId = "asset-0";
    document.nodes.push_back(std::move(root));
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
        || schemaVersion < 1 || schemaVersion > kSchemaVersion) {
        throw std::runtime_error("Unsupported .azscene schemaVersion");
    }
    std::uint32_t renderSettingsVersion = 1;
    if (!(input >> key)) {
        throw std::runtime_error("Missing .azscene sceneId");
    }
    if (key == "renderSettingsVersion") {
        if (!(input >> renderSettingsVersion >> key)) {
            throw std::runtime_error(
                "Invalid .azscene renderSettingsVersion");
        }
    }
    if (key != "sceneId" || !(input >> std::quoted(document.sceneId))) {
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
        if (schemaVersion >= 2) {
            if (!(input >> key
                  >> node.translation[0] >> node.translation[1]
                  >> node.translation[2] >> node.rotation[0]
                  >> node.rotation[1] >> node.rotation[2]
                  >> node.scale[0] >> node.scale[1] >> node.scale[2])
                || key != "transform") {
                throw std::runtime_error("Invalid .azscene node transform");
            }
            if (!(input >> key >> std::quoted(node.prefabSource)
                  >> std::quoted(node.instanceOf)) || key != "prefab") {
                throw std::runtime_error("Invalid .azscene prefab instance");
            }
        }
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
    // Blackhole settings and sceneRenderer are optional for backward
    // compatibility with schema v1 documents.
    // documents that predate the pluggable scene renderer.
    if (!(input >> key)) {
        // Reached end of file: keep the default Character renderer.
        document.renderSettings.sceneType = SceneType::Character;
    } else {
        if (key == "characterBackgroundEnabled") {
            if (!(input >> std::boolalpha
                  >> document.renderSettings.characterPresentation.backgroundEnabled
                  >> key)) {
                throw std::runtime_error(
                    "Invalid .azscene characterBackgroundEnabled");
            }
        }
        if (key == "characterPlatformEnabled") {
            if (!(input >> std::boolalpha
                  >> document.renderSettings.characterPresentation.platformEnabled
                  >> key)) {
                throw std::runtime_error(
                    "Invalid .azscene characterPlatformEnabled");
            }
        }
        if (key == "blackholeQuality") {
            std::string value;
            if (!(input >> value)) {
                throw std::runtime_error("Invalid .azscene blackholeQuality");
            }
            if (value == "performance") {
                document.renderSettings.blackhole.quality = BlackholeQuality::Performance;
            } else if (value == "balanced") {
                document.renderSettings.blackhole.quality = BlackholeQuality::Balanced;
            } else if (value != "cinematic") {
                throw std::runtime_error("Unsupported .azscene blackholeQuality: " + value);
            }
            if (!(input >> key)) {
                throw std::runtime_error("Missing .azscene blackholeCamera");
            }
        }
        if (key == "blackholeCamera") {
            std::string value;
            if (!(input >> value)) {
                throw std::runtime_error("Invalid .azscene blackholeCamera");
            }
            if (value == "orbit-left") {
                document.renderSettings.blackhole.camera = BlackholeCamera::OrbitLeft;
            } else if (value == "high") {
                document.renderSettings.blackhole.camera = BlackholeCamera::High;
            } else if (value == "close") {
                document.renderSettings.blackhole.camera = BlackholeCamera::Close;
            } else if (value == "over-shoulder") {
                document.renderSettings.blackhole.camera = BlackholeCamera::OverShoulder;
            } else if (value != "front") {
                throw std::runtime_error("Unsupported .azscene blackholeCamera: " + value);
            }
            if (!(input >> key)) {
                throw std::runtime_error("Missing .azscene sceneRenderer");
            }
        }
        if (key != "sceneRenderer") {
            throw std::runtime_error("Invalid .azscene trailing field: " + key);
        }
        std::string sceneTypeValue;
        if (!(input >> sceneTypeValue)) {
            throw std::runtime_error("Invalid .azscene sceneRenderer");
        }
        try {
            document.renderSettings.sceneType =
                sceneTypeFromString(sceneTypeValue);
        } catch (const std::invalid_argument&) {
            throw std::runtime_error(
                "Unsupported .azscene sceneRenderer: " + sceneTypeValue);
        }
    }
    document.renderSettings = migrateRenderSettings(
        document.renderSettings, renderSettingsVersion);
    return document;
}

void SceneDocument::save(const std::filesystem::path& path) const {
    validateRenderSettings(renderSettings);
    if (path.empty()) {
        throw std::invalid_argument("Scene save path must not be empty");
    }
    const std::filesystem::path directory =
        path.parent_path().empty() ? std::filesystem::path(".")
                                   : path.parent_path();
    std::error_code directoryError;
    if (!std::filesystem::is_directory(directory, directoryError)
        || directoryError) {
        throw std::runtime_error(
            "Scene save directory does not exist: " + directory.string());
    }

    // Atomic save: write a unique temporary file in the same directory, flush
    // and close it, then rename over the target. A failure at any stage leaves
    // the original target untouched and removes the temporary file.
    static std::mt19937_64 generator{
        static_cast<std::uint64_t>(std::chrono::steady_clock::now()
                                      .time_since_epoch().count())};
    const std::string suffix =
        ".tmp." + std::to_string(generator());
    const std::filesystem::path temporary =
        directory / (path.filename().string() + suffix);

    try {
        {
            std::ofstream output(temporary,
                std::ios::binary | std::ios::trunc);
            if (!output) {
                throw std::runtime_error(
                    "Unable to write scene: " + temporary.string());
            }
            writeDocument(output, *this);
            output.flush();
            if (!output) {
                throw std::runtime_error(
                    "Failed to flush scene: " + temporary.string());
            }
        }
        std::error_code renameError;
        std::filesystem::rename(temporary, path, renameError);
        if (renameError) {
            throw std::runtime_error(
                "Unable to replace scene: " + path.string()
                + " (" + renameError.message() + ")");
        }
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        throw;
    }
}

}  // namespace azurerender
