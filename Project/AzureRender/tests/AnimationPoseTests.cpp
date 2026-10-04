#include "assets/GltfLoader.hpp"
#include <cmath>
#include <future>
#include <iostream>
#include <stdexcept>

void check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
int main() {
    try {
        LoadedAsset asset;
        asset.hasSkin = true;
        asset.nodes.resize(2);
        asset.nodes[1].parent = 0;
        asset.nodes[1].translation = {0, 1, 0};
        asset.jointNodes = {1};
        std::array<float, 16> identity{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
        asset.inverseBindMatrices = {identity};
        asset.nodeWorldMatrices = {identity, identity};
        AssetAnimation idle; idle.name = "idle"; idle.endTime = 1;
        idle.samplers.push_back({{0,1}, {{0,1,0,0},{2,1,0,0}}});
        idle.channels.push_back({0,1,AssetAnimationPath::Translation});
        AssetAnimation walk = idle; walk.name = "walk";
        walk.samplers[0].outputValues = {{4,1,0,0},{6,1,0,0}};
        asset.animations = {idle, walk};
        const auto original = asset.nodeWorldMatrices;
        const LoadedAsset& immutable = asset;
        auto first = sampleAnimationPose(immutable, 0, .25F);
        auto second = sampleAnimationPose(immutable, 1, .75F);
        check(std::abs(first.jointMatrices[0][12]-.5F)<1e-6F, "First instance must sample its own time");
        check(std::abs(second.jointMatrices[0][12]-5.5F)<1e-6F, "Second instance must sample another clip");
        check(asset.nodeWorldMatrices == original, "Shared skeleton must stay immutable");
        auto blended = blendAnimationPoses(immutable, first, second, .25F);
        check(std::abs(blended.jointMatrices[0][12]-1.75F)<1e-6F, "Blend must interpolate local poses");
        auto clamped = sampleAnimationPose(immutable, 0, 2, false);
        check(std::abs(clamped.jointMatrices[0][12]-2)<1e-6F, "Nonloop clips must hold their last key");
        auto worker = std::async(std::launch::async, [&] { return sampleAnimationPose(immutable, 1, .25F); });
        auto concurrent = sampleAnimationPose(immutable, 0, .75F);
        check(std::abs(worker.get().jointMatrices[0][12]-4.5F)<1e-6F && std::abs(concurrent.jointMatrices[0][12]-1.5F)<1e-6F, "Concurrent instances must stay independent");
        bool rejected = false;
        try { (void)sampleAnimationPose(immutable, 4, 0); } catch (const std::exception&) { rejected = true; }
        check(rejected, "Missing clip must be rejected");
        std::cout << "Immutable sampling, independent instances, local blend and nonloop endpoint passed\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
