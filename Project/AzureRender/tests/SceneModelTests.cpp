#include "editor/SceneModel.hpp"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <cmath>
#include <stdexcept>
#include <string>

namespace {

std::filesystem::path uniquePath(const std::string& suffix) {
    const auto stamp = std::chrono::steady_clock::now()
        .time_since_epoch().count();
    return std::filesystem::temp_directory_path()
        / ("azurerender-scene-" + std::to_string(stamp) + suffix);
}

std::filesystem::path uniqueDirectory() {
    const auto stamp = std::chrono::steady_clock::now()
        .time_since_epoch().count();
    const auto directory = std::filesystem::temp_directory_path()
        / ("azurerender-scene-dir-" + std::to_string(stamp));
    std::filesystem::create_directories(directory);
    return directory;
}

// Counts leftover temporary files (any file whose name contains ".tmp.").
std::size_t countTemporaryFiles(const std::filesystem::path& directory) {
    std::size_t count = 0;
    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (entry.is_regular_file()
            && entry.path().filename().string().find(".tmp.") != std::string::npos) {
            ++count;
        }
    }
    return count;
}

bool loadedDocumentMatches(const std::filesystem::path& path,
                           const azurerender::SceneDocument& expected) {
    const auto loaded = azurerender::SceneDocument::load(path);
    return loaded.sceneId == expected.sceneId
        && loaded.resources.size() == expected.resources.size()
        && loaded.nodes.size() == expected.nodes.size()
        && loaded.renderSettings.showcasePreset
            == expected.renderSettings.showcasePreset
        && loaded.renderSettings.shadow.maximumFilterRadiusTexels
            == expected.renderSettings.shadow.maximumFilterRadiusTexels;
}

}  // namespace

int main() {
    // The data-driven look catalog rejects incompatible schemas before any
    // renderer state is changed.
    {
        const auto invalidCatalog = uniquePath("-looks.json");
        {
            std::ofstream output(invalidCatalog);
            output << R"({"schemaVersion":999,"looks":[]})";
        }
        bool rejected = false;
        try {
            azurerender::loadShowcasePresetCatalog(invalidCatalog);
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        assert(rejected);
        std::filesystem::remove(invalidCatalog);
    }
    azurerender::loadShowcasePresetCatalog(
        std::filesystem::path(AZURERENDER_TEST_SOURCE_DIR)
        / "assets_public/showcase_looks.json");
    // Showcase presets are stable names and complete visual configurations.
    {
        azurerender::RenderSettings settings;
        azurerender::applyShowcasePresetLook(settings, 1);
        static_assert(azurerender::kShowcasePresetVersion == 1);
        assert(azurerender::showcasePresetName(1) == "Endfield Industrial");
        assert(settings.showcasePreset == 1);
        assert(std::abs(settings.grade.exposureEv - 0.12F) < 0.0001F);
        assert(std::abs(settings.grade.saturation - 0.96F) < 0.0001F);
        assert(std::abs(settings.grade.contrast - 1.04F) < 0.0001F);
        assert(std::abs(settings.bloom.strength - 0.10F) < 0.0001F);
        assert(std::abs(settings.outline.strength - 0.42F) < 0.0001F);
        azurerender::validateRenderSettings(settings);
    }

    // 1. A normal save produces a loadable file and leaves no temporary file.
    {
        const auto directory = uniqueDirectory();
        const auto scenePath = directory / "scene.azscene";
        auto document = azurerender::SceneDocument::fromAsset("hero.glb");
        document.sceneId = "atomic-save-test";
        document.save(scenePath);
        {
            std::ifstream saved(scenePath);
            const std::string contents(
                (std::istreambuf_iterator<char>(saved)),
                std::istreambuf_iterator<char>());
            assert(contents.find("renderSettingsVersion 7")
                != std::string::npos);
            assert(contents.find("shadowMaximumFilterRadiusTexels 8")
                != std::string::npos);
        }
        assert(std::filesystem::exists(scenePath));
        assert(loadedDocumentMatches(scenePath, document));
        assert(countTemporaryFiles(directory) == 0);
        std::filesystem::remove_all(directory);
    }

    // 8. Settings migration rejects future schemas and defaults legacy
    //    renderer selection without weakening current validation.
    {
        azurerender::RenderSettings settings;
        settings.sceneType = azurerender::SceneType::Blackhole;
        const auto migrated = azurerender::migrateRenderSettings(settings, 3);
        assert(migrated.sceneType == azurerender::SceneType::Character);
        bool rejected = false;
        try {
            (void)azurerender::migrateRenderSettings(
                settings, azurerender::RenderSettings::kSchemaVersion + 1);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        assert(rejected);
    }

    // 9. Quality profiles are stable and temporal history resets exactly when
    //    a sampling or camera contract changes.
    {
        const auto cinematic = azurerender::blackholeQualityParameters(
            azurerender::BlackholeQuality::Cinematic);
        assert(cinematic.maxTraceSteps == 1800);
        assert(cinematic.samplesPerPixel == 4);
        azurerender::BlackholeSettings previous;
        azurerender::BlackholeSettings current = previous;
        assert(!azurerender::blackholeHistoryNeedsReset(previous, current));
        current.camera = azurerender::BlackholeCamera::OverShoulder;
        assert(azurerender::blackholeHistoryNeedsReset(previous, current));
        current = previous;
        current.quality = azurerender::BlackholeQuality::Balanced;
        assert(azurerender::blackholeHistoryNeedsReset(previous, current));
    }

    // 2. Re-saving over an existing file replaces it atomically and cleanly.
    {
        const auto directory = uniqueDirectory();
        const auto scenePath = directory / "overwrite.azscene";
        auto first = azurerender::SceneDocument::fromAsset("hero.glb");
        first.sceneId = "first";
        first.save(scenePath);
        auto second = azurerender::SceneDocument::fromAsset("mecha.glb");
        second.sceneId = "second";
        second.save(scenePath);
        assert(loadedDocumentMatches(scenePath, second));
        assert(countTemporaryFiles(directory) == 0);
        std::filesystem::remove_all(directory);
    }

    // 3. A failed save to an unwritable directory keeps any pre-existing
    //    target intact and leaves no temporary file behind.
    {
        const auto directory = uniqueDirectory();
        const auto scenePath = directory / "kept.azscene";
        auto document = azurerender::SceneDocument::fromAsset("hero.glb");
        document.sceneId = "kept";
        document.save(scenePath);
        const auto original = std::filesystem::file_size(scenePath);

        // A path whose parent is a regular file is never a writable directory.
        const auto blocker = directory / "not-a-directory";
        {
            std::ofstream blockerStream(blocker);
            blockerStream << "occupied";
        }
        const auto blockedPath = blocker / "scene.azscene";
        bool threw = false;
        try {
            document.save(blockedPath);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        assert(threw);
        // The pre-existing target is untouched and no temporary file remains.
        assert(std::filesystem::exists(scenePath));
        assert(std::filesystem::file_size(scenePath) == original);
        assert(countTemporaryFiles(directory) == 0);
        std::filesystem::remove_all(directory);
    }

    // 4. A save into a missing parent directory fails without side effects.
    {
        const auto directory = uniqueDirectory();
        const auto scenePath = directory / "missing" / "scene.azscene";
        auto document = azurerender::SceneDocument::fromAsset("hero.glb");
        bool threw = false;
        try {
            document.save(scenePath);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        assert(threw);
        assert(!std::filesystem::exists(scenePath));
        assert(countTemporaryFiles(directory) == 0);
        std::filesystem::remove_all(directory);
    }

    // 5. Save-then-load round trip preserves settings beyond the first fields.
    {
        const auto directory = uniqueDirectory();
        const auto scenePath = directory / "settings.azscene";
        auto document = azurerender::SceneDocument::fromAsset("hero.glb");
        document.renderSettings.styleMaskStrength = 0.55f;
        document.renderSettings.innerOutlineEnabled = false;
        document.save(scenePath);
        const auto loaded = azurerender::SceneDocument::load(scenePath);
        assert(loaded.renderSettings.styleMaskStrength
            == document.renderSettings.styleMaskStrength);
        assert(loaded.renderSettings.innerOutlineEnabled
            == document.renderSettings.innerOutlineEnabled);
        assert(countTemporaryFiles(directory) == 0);
        std::filesystem::remove_all(directory);
    }

    // 6. Scene renderer selector survives a save-then-load round trip.
    {
        const auto directory = uniqueDirectory();
        const auto scenePath = directory / "renderer.azscene";
        auto document = azurerender::SceneDocument::fromAsset("hero.glb");
        document.renderSettings.sceneType =
            azurerender::SceneType::Blackhole;
        document.renderSettings.blackhole.quality =
            azurerender::BlackholeQuality::Balanced;
        document.renderSettings.blackhole.camera =
            azurerender::BlackholeCamera::OverShoulder;
        document.renderSettings.characterPresentation.backgroundEnabled = false;
        document.renderSettings.characterPresentation.platformEnabled = false;
        azurerender::SceneNode instance;
        instance.id = "hero-instance";
        instance.name = "Hero Instance";
        instance.parentId = "root";
        instance.resourceId = "asset-0";
        instance.translation = {1.0F, 2.0F, 3.0F};
        instance.rotation = {0.0F, 45.0F, 0.0F};
        instance.scale = {0.5F, 0.5F, 0.5F};
        instance.prefabSource = "prefabs/hero.azprefab";
        instance.instanceOf = "hero-template";
        document.nodes.push_back(instance);
        document.save(scenePath);
        const auto loaded = azurerender::SceneDocument::load(scenePath);
        assert(loaded.renderSettings.sceneType
            == azurerender::SceneType::Blackhole);
        assert(loaded.renderSettings.blackhole.quality
            == azurerender::BlackholeQuality::Balanced);
        assert(loaded.renderSettings.blackhole.camera
            == azurerender::BlackholeCamera::OverShoulder);
        assert(!loaded.renderSettings.characterPresentation.backgroundEnabled);
        assert(!loaded.renderSettings.characterPresentation.platformEnabled);
        assert(loaded.nodes.size() == 2);
        assert(loaded.nodes[1].translation == instance.translation);
        assert(loaded.nodes[1].prefabSource == instance.prefabSource);
        assert(loaded.nodes[1].instanceOf == instance.instanceOf);
        assert(countTemporaryFiles(directory) == 0);
        std::filesystem::remove_all(directory);
    }

    // 7. Schema v1 documents without a sceneRenderer field default to
    //    Character instead of failing.
    {
        const auto directory = uniqueDirectory();
        const auto scenePath = directory / "legacy.azscene";
        {
            std::ofstream legacy(scenePath);
            legacy << "AzureRender Scene v1\n"
                   << "schemaVersion 1\n"
                   << "sceneId \"legacy\"\n"
                   << "resourceCount 1\n"
                   << "resource \"asset-0\" \"gltf\" \"hero.glb\"\n"
                   << "nodeCount 1\n"
                   << "node \"root\" \"legacy\" \"\" \"asset-0\"\n"
                   << "visible true\n"
                   << "showcasePreset 0\n"
                   << "styleMaskStrength 1\n"
                   << "diffuseBandThreshold 0.4\n"
                   << "innerOutlineEnabled true\n"
                   << "outlineStrength 0.4\n"
                   << "gradeExposureEv 0\n";
        }
        const auto loaded = azurerender::SceneDocument::load(scenePath);
        assert(loaded.renderSettings.sceneType
            == azurerender::SceneType::Character);
        assert(countTemporaryFiles(directory) == 0);
        std::filesystem::remove_all(directory);
    }

    return 0;
}
