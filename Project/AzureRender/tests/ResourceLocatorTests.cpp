#include "resources/ResourceLocator.hpp"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>

int main() {
    const auto root = std::filesystem::temp_directory_path()
        / ("azurerender-resources-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root / "shaders");
    std::filesystem::create_directories(root / "assets_public");
    std::ofstream(root / "shaders/mesh.vert.spv") << "test";
    std::ofstream(root / "assets_public/test_model.gltf") << "test";
    std::ofstream(root / "assets_public/toon_ramp_profiles.json") << "{}";
    std::ofstream(root / "assets_public/toon_ramp_atlas.ppm") << "P6";

    const azurerender::ResourceLocator locator(root);
    assert(locator.shaderDirectory() == std::filesystem::weakly_canonical(root / "shaders"));
    assert(locator.publicAsset("test_model.gltf")
           == std::filesystem::weakly_canonical(root / "assets_public/test_model.gltf"));
    assert(locator.rampProfile().filename() == "toon_ramp_profiles.json");
    assert(locator.rampAtlas().filename() == "toon_ramp_atlas.ppm");

    bool missingReported = false;
    try {
        static_cast<void>(locator.publicAsset("missing.gltf"));
    } catch (const std::runtime_error& error) {
        missingReported = std::string(error.what()).find("searched:")
            != std::string::npos;
    }
    assert(missingReported);
    std::filesystem::remove_all(root);
    return 0;
}
