#include "assets/BrowRegions.hpp"
#include <cmath>
#include <cstdio>
#include <stdexcept>

int main(int argc, char** argv) try {
    if (argc == 2) {
        const auto loaded = loadGltfAsset(argv[1]);
        const auto selected = std::count_if(loaded.vertices.begin(), loaded.vertices.end(),
            [](const auto& v) { return v.browMask > 0.5F; });
        std::printf("Loaded brow region vertices: %zu\n", static_cast<std::size_t>(selected));
        if (selected == 0) throw std::runtime_error("Actual character has no named eyebrow regions");
    }
    LoadedAsset asset;
    asset.nodes.resize(2);
    asset.nodes[0].name = "eyeEyelash";
    asset.nodes[1].name = "browRight";
    asset.jointNodes = {0, 1};
    asset.vertices.resize(6);
    for (unsigned i = 0; i < 6; ++i) {
        asset.vertices[i].position = {float(i % 3 == 1), float(1 + (i % 3 == 2)), 0};
        asset.vertices[i].joints[0] = i < 3 ? 1 : 0;
    }
    asset.indices = {0, 1, 2, 3, 4, 5};
    asset.materials.resize(1);
    asset.materials[0].materialFeatures = MaterialFeatureBrowOverlay;
    asset.materials[0].styleParameters[0] = 0.001F;
    asset.primitives.push_back({0, 6, 0, {}});
    const auto lashes = asset.vertices[3].position;
    const auto count = azurerender::prepareBrowRegions(asset);
    if (count != 1 || asset.vertices[0].browMask != 1 || asset.vertices[3].browMask != 0
        || std::abs(asset.vertices[0].position[1] - 0.999F) > 1e-6F
        || std::abs(asset.vertices[0].position[0] + 0.00045F) > 1e-6F
        || asset.vertices[3].position != lashes) {
        throw std::runtime_error("Named eyebrow island expansion must preserve eyelashes");
    }
    // Reordering the skin must preserve the same region selection.
    asset.jointNodes = {1, 0};
    for (unsigned i = 0; i < 6; ++i) asset.vertices[i].joints[0] = i < 3 ? 0 : 1;
    if (azurerender::prepareBrowRegions(asset) != 1 || asset.vertices[3].position != lashes)
        throw std::runtime_error("Region selection depends on a joint number");
    std::puts("Brow named regions and eyelash preservation passed");
    return 0;
} catch (const std::exception& error) { std::fprintf(stderr, "%s\n", error.what()); return 1; }
