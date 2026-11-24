#include "resources/ResourceLocator.hpp"
#include "resources/PreparedResourcePaths.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, int line) {
    if (!condition) throw std::runtime_error("Resource path contract failed at line " + std::to_string(line));
}
}
#define check(...) require((__VA_ARGS__), __LINE__)

int main() try {
    const auto root = azurerender::ResourceLocator::executableDirectory()/"test-artifacts"
        / ("azurerender-resources-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root / "shaders");
    std::filesystem::create_directories(root / "assets_public");
    std::ofstream(root / "shaders/mesh.vert.spv") << "test";
    std::ofstream(root / "assets_public/test_model.gltf") << "test";
    std::ofstream(root / "assets_public/toon_ramp_profiles.json") << "{}";
    std::ofstream(root / "assets_public/toon_ramp_atlas.ppm") << "P6";
    std::ofstream(root / "assets_public/showcase_looks.json") << "{}";

    const azurerender::ResourceLocator locator(root);
    check(locator.shaderDirectory() == std::filesystem::weakly_canonical(root / "shaders"));
    check(locator.publicAsset("test_model.gltf")
           == std::filesystem::weakly_canonical(root / "assets_public/test_model.gltf"));
    check(locator.rampProfile().filename() == "toon_ramp_profiles.json");
    check(locator.showcaseLooks().filename() == "showcase_looks.json");
    check(locator.rampAtlas().filename() == "toon_ramp_atlas.ppm");
    check(locator.generatedCacheDirectory({}).empty());
    const auto firstCache=locator.generatedCacheDirectory(root/"one"/"project.azureproject");
    const auto secondCache=locator.generatedCacheDirectory(root/"two"/"project.azureproject");
    check(firstCache!=secondCache);
    check(firstCache==std::filesystem::absolute(root/"one"/".azure"/"cache"));
    check(firstCache.is_absolute()&&firstCache.filename()=="cache");

    bool missingReported = false;
    try {
        static_cast<void>(locator.publicAsset("missing.gltf"));
    } catch (const std::runtime_error& error) {
        missingReported = std::string(error.what()).find("searched:")
            != std::string::npos;
    }
    check(missingReported);

    const auto asset = std::filesystem::weakly_canonical(root / "assets_public/test_model.gltf");
    const auto second = std::filesystem::weakly_canonical(root / "assets_public/second.gltf");
    std::ofstream(second) << "second";
    azurerender::PreparedResourcePaths paths(locator);
    const auto requested = std::vector<std::string>{"test_model.gltf", "test_model.gltf"};
    check(paths.resolve(requested) == std::vector<std::string>({asset.u8string(), asset.u8string()}));
    std::filesystem::remove(asset);
    check(paths.resolve(requested) == std::vector<std::string>({asset.u8string(), asset.u8string()}));
    bool directMissing = false;
    try { static_cast<void>(locator.resolveAsset("test_model.gltf")); }
    catch (const std::runtime_error&) { directMissing = true; }
    check(directMissing);

    // A failed batch must retain the confirmed snapshot and omit partial results.
    bool batchMissing = false;
    try { static_cast<void>(paths.resolve({"second.gltf", "missing.gltf"})); }
    catch (const std::runtime_error&) { batchMissing = true; }
    check(batchMissing);
    std::filesystem::remove(second);
    check(paths.resolve(requested).front() == asset.u8string());
    bool partialMissing = false;
    try { static_cast<void>(paths.resolve({"second.gltf"})); }
    catch (const std::runtime_error&) { partialMissing = true; }
    check(partialMissing);

    paths.clear();
    bool reloadMissing = false;
    try { static_cast<void>(paths.resolve(requested)); }
    catch (const std::runtime_error&) { reloadMissing = true; }
    check(reloadMissing);
    std::ofstream(asset) << "restored";
    check(paths.resolve(requested).front() == asset.u8string());

    // Unreferenced paths must leave the active table.
    std::ofstream(second) << "second";
    check(paths.resolve({"second.gltf"}).front() == second.u8string());
    std::filesystem::remove(asset);
    bool prunedMissing = false;
    try { static_cast<void>(paths.resolve(requested)); }
    catch (const std::runtime_error&) { prunedMissing = true; }
    check(prunedMissing);
    check(paths.resolve({}).empty());
    std::filesystem::remove(second);
    bool emptyPrunedMissing = false;
    try { static_cast<void>(paths.resolve({"second.gltf"})); }
    catch (const std::runtime_error&) { emptyPrunedMissing = true; }
    check(emptyPrunedMissing);

    const auto otherRoot = root / "other";
    std::filesystem::create_directories(otherRoot / "assets_public");
    std::ofstream(otherRoot / "assets_public/test_model.gltf") << "other";
    const auto otherAsset = std::filesystem::weakly_canonical(otherRoot / "assets_public/test_model.gltf");
    std::ofstream(asset) << "first root";
    check(paths.resolve({"test_model.gltf"}).front() == asset.u8string());
    azurerender::PreparedResourcePaths inspector{azurerender::ResourceLocator(otherRoot)};
    check(inspector.resolve({"test_model.gltf"}).front() == otherAsset.u8string());
    check(paths.resolve({"test_model.gltf"}).front() == asset.u8string());
    check(inspector.resolve({otherAsset.u8string()}).front() == otherAsset.u8string());
    std::filesystem::remove_all(root);
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
