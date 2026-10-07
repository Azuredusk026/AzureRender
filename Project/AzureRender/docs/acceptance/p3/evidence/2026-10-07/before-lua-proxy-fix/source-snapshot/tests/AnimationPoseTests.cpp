#include "assets/GltfLoader.hpp"
#include "render/DeformedBounds.hpp"
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
        // Per-joint envelopes must contain a weighted, morphed vertex after
        // arbitrary affine poses, including negative morph weights.
        LoadedAsset bounded;
        bounded.hasSkin=true; bounded.jointMatrices={identity,identity};
        AssetVertex vertex{}; vertex.position={2,3,4}; vertex.morph0={1,-2,3};
        vertex.joints={0,1,0,0}; vertex.weights={.25F,.75F,0,0};
        bounded.vertices={vertex};
        auto matrices=bounded.jointMatrices;matrices[0][12]=8;matrices[1][13]=-4;
        const auto envelopes=azurerender::JointBounds::build(bounded);
        auto envelope=envelopes.evaluate(bounded,{-2,0},matrices);
        check(envelope.minimum[0]<=2 && envelope.maximum[0]>=2
            && envelope.minimum[1]<=4 && envelope.maximum[1]>=4
            && envelope.minimum[2]<=-2 && envelope.maximum[2]>=-2,
            "Joint envelope must contain weighted transformed morph position");
        azurerender::scene::SceneDescription description;
        description.resources={{"hero","C:/models/hero.glb"},{"box","C:/models/box.gltf"},
            {"alias","C:/models/./box.gltf"},{"hero-copy","C:/models/hero.glb"}};
        azurerender::scene::ResourceLayout layout(description);
        check(layout.paths.size()==2 && layout.meshKey("alias")==1 && layout.meshKey("hero-copy")==0,
            "Aliases must share a mesh while preserving resource identity");
        std::cout << "Immutable sampling, independent instances, local blend and nonloop endpoint passed\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
