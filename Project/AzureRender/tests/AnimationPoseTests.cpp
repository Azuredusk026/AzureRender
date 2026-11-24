#include "assets/GltfLoader.hpp"
#include "render/DeformedBounds.hpp"
#include "animation/AnimationClipSample.hpp"
#include <cstring>
#include <nlohmann/json.hpp>
#include <cmath>
#include <future>
#include <iostream>
#include <stdexcept>

void check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
std::string rigidFixture(bool mixedSkin=false) {
    std::string json=R"({"asset":{"version":"2.0"},"buffers":[{"byteLength":92}],"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":8},{"buffer":0,"byteOffset":44,"byteLength":24},{"buffer":0,"byteOffset":68,"byteLength":24}],"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[1,1,0]},{"bufferView":1,"componentType":5126,"count":2,"type":"SCALAR","min":[0],"max":[2]},{"bufferView":2,"componentType":5126,"count":2,"type":"VEC3"},{"bufferView":3,"componentType":5126,"count":2,"type":"VEC3"}],"meshes":[{"primitives":[{"attributes":{"POSITION":0},"mode":4}]}],"nodes":[{"translation":[2,0,0],"children":[1]},{"mesh":0,"translation":[0,3,0]},{"mesh":0,"translation":[-5,0,0]}],"scenes":[{"nodes":[0,2]}],"scene":0,"animations":[{"samplers":[{"input":1,"output":2},{"input":1,"output":3,"interpolation":"STEP"}],"channels":[{"sampler":0,"target":{"node":0,"path":"translation"}},{"sampler":1,"target":{"node":1,"path":"scale"}}]}]})";
    const float values[]={0,0,0, 1,0,0, 0,1,0, 0,2, 2,0,0, 4,0,0, 1,1,1, 0,0,0};
    std::string binary(reinterpret_cast<const char*>(values),sizeof(values));
    if(mixedSkin) {
        auto source=nlohmann::json::parse(json);
        source["skins"]=nlohmann::json::array({{{"joints",{0}}}});
        source["nodes"][2]["skin"]=0;
        source["bufferViews"].push_back({{"buffer",0},{"byteOffset",92},{"byteLength",12}});
        source["bufferViews"].push_back({{"buffer",0},{"byteOffset",104},{"byteLength",48}});
        source["accessors"].push_back({{"bufferView",4},{"componentType",5121},{"count",3},{"type","VEC4"}});
        source["accessors"].push_back({{"bufferView",5},{"componentType",5126},{"count",3},{"type","VEC4"}});
        source["meshes"][0]["primitives"][0]["attributes"]["JOINTS_0"]=4;
        source["meshes"][0]["primitives"][0]["attributes"]["WEIGHTS_0"]=5;
        binary.append(12,'\0');
        const float weights[]={1,0,0,0,1,0,0,0,1,0,0,0};
        binary.append(reinterpret_cast<const char*>(weights),sizeof(weights));
        source["buffers"][0]["byteLength"]=binary.size();json=source.dump();
    }
    while(json.size()%4)json+=' ';
    std::string result;
    const auto append=[&](std::uint32_t word){result.append(reinterpret_cast<const char*>(&word),4);};
    append(0x46546c67);append(2);append(static_cast<std::uint32_t>(28+json.size()+binary.size()));
    append(static_cast<std::uint32_t>(json.size()));append(0x4e4f534a);result+=json;
    append(static_cast<std::uint32_t>(binary.size()));append(0x004e4942);result+=binary;
    return result;
}
int main() {
    try {
        const auto rigid=loadEmbeddedGltfAsset(rigidFixture(),true);
        const auto weighted=sampleAnimationBlend(rigid,{{0,0,.25F,false,2},{0,2,.75F,false,2}});
        check(std::abs(weighted.local[0].translation[0]-3.5F)<1e-6F,"Weighted clips blend in local space");
        check(std::abs(weighted.local[1].scale[0]-.25F)<1e-6F,"Weighted scale reaches the resolved pose");
        for(int kind=0;kind<4;++kind){
            std::vector<azurerender::AnimationClipSample> samples{{0,0,1,false,2}};
            if(kind==0)samples[0].clip=9;
            if(kind==1)samples[0].weight=.5F;
            if(kind==2)samples[0].duration=3;
            if(kind==3)samples[0].weight=-1;
            bool rejected=false;try{(void)sampleAnimationBlend(rigid,samples);}catch(const std::exception&){rejected=true;}
            check(rejected,"Invalid weighted frame or bound duration rejects");
        }
        const auto rigidBind=bindAnimationPose(rigid);
        const auto moving=sampleAnimationPose(rigid,0,1,false);
        const auto moved=azurerender::deformedVertexPosition(rigid,0,&moving,{0,0});
        check(std::abs(moved[0]-3)<1e-6F && std::abs(moved[1]-3)<1e-6F,
            "Unskinned mesh must follow its animated parent on the deformation path");
        check(azurerender::deformedVertexPosition(rigid,3,&moving,{0,0})==std::array<float,3>{-5,0,0},
            "Independent static mesh must preserve its bind transform");
        const auto vanished=sampleAnimationPose(rigid,0,2,false);
        check(azurerender::deformedVertexPosition(rigid,0,&vanished,{0,0})==azurerender::deformedVertexPosition(rigid,1,&vanished,{0,0}),
            "STEP zero scale must collapse rigid geometry at the lifetime boundary");
        check(azurerender::deformedVertexPosition(rigid,0,&rigidBind,{0,0})==std::array<float,3>{2,3,0},
            "Rigid bind pose must preserve authored placement");
        const auto rigidBounds=azurerender::JointBounds::build(rigid).evaluate(rigid,{0,0},moving.jointMatrices,&moving);
        check(rigidBounds.maximum[0]>=4 && rigidBounds.minimum[0]<=-5,
            "Animated rigid geometry must contribute its current bounds");
        const auto mixed=loadEmbeddedGltfAsset(rigidFixture(true),true);
        const auto mixedPose=sampleAnimationPose(mixed,0,1,false);
        check(mixed.vertices[0].joints[0]==1 && mixed.vertices[3].joints[0]==0,
            "Rigid palette entries must preserve real skin joint indices");
        check(azurerender::deformedVertexPosition(mixed,0,&mixedPose,{0,0})==std::array<float,3>{3,3,0}
            && azurerender::deformedVertexPosition(mixed,3,&mixedPose,{0,0})==std::array<float,3>{3,0,0},
            "A mixed asset must animate rigid children and real skin through one palette");
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
        auto expressive=asset;expressive.defaultMorphWeights={{0,0},{0,0}};
        for(std::size_t i=0;i<expressive.animations.size();++i){
            auto& clip=expressive.animations[i];
            const float sign=i==0?1.F:-1.F;
            clip.samplers.push_back({{0,1},{{0,0,0,sign},{0,0,0,sign}}});
            clip.channels.push_back({1,1,AssetAnimationPath::Rotation});
            AssetAnimationSampler morph;
            morph.curve={{0,1},{0,0,1,static_cast<float>(i)},2};
            clip.samplers.push_back(morph);clip.channels.push_back({2,1,AssetAnimationPath::Weights});
        }
        const auto expressivePose=sampleAnimationBlend(expressive,{{0,1,.5F,false,1},{1,1,.5F,false,1}});
        check(std::abs(expressivePose.local[1].rotation[3]-1)<1e-6F,
            "Opposite quaternion signs must preserve orientation when blending");
        check(expressivePose.morphWeights[1]==MorphWeightState{1,.5F},
            "Weighted morph channels must reach the shared resolved pose");
        check(std::abs(expressivePose.jointMatrices[0][12]-4)<1e-6F,
            "Weighted translation must resolve through the skin palette");
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
