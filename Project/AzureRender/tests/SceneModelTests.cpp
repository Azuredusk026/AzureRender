#include "editor/SceneModel.hpp"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
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
            == expected.renderSettings.showcasePreset;
}

}  // namespace

int main() {
    // 1. A normal save produces a loadable file and leaves no temporary file.
    {
        const auto directory = uniqueDirectory();
        const auto scenePath = directory / "scene.azscene";
        auto document = azurerender::SceneDocument::fromAsset("hero.glb");
        document.sceneId = "atomic-save-test";
        document.save(scenePath);
        assert(std::filesystem::exists(scenePath));
        assert(loadedDocumentMatches(scenePath, document));
        assert(countTemporaryFiles(directory) == 0);
        std::filesystem::remove_all(directory);
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
        document.save(scenePath);
        const auto loaded = azurerender::SceneDocument::load(scenePath);
        assert(loaded.renderSettings.sceneType
            == azurerender::SceneType::Blackhole);
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
