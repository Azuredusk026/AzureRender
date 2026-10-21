#include "assets/GltfLoader.hpp"
#include "assets/AssetDeformation.hpp"
#include "animation/ExpressionPose.hpp"
#include <nlohmann/json.hpp>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using Json = nlohmann::json;
void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
struct Fixture {
    Json document = Json::parse(R"({"asset":{"version":"2.0"},"buffers":[{"byteLength":36}],"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":36}],"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3","min":[0,0,0],"max":[1,1,0]}],"meshes":[{"primitives":[{"attributes":{"POSITION":0},"mode":4}]}],"nodes":[{"children":[1]},{"mesh":0}],"scenes":[{"nodes":[0]}],"scene":0})");
    std::string binary;
    Fixture() {
        const float vertices[] = {0,0,0, 1,0,0, 0,1,0};
        binary.assign(reinterpret_cast<const char*>(vertices), sizeof(vertices));
    }
    int scalars(const std::vector<float>& values) {
        const auto offset = binary.size();
        binary.append(reinterpret_cast<const char*>(values.data()), values.size() * sizeof(float));
        const int view = static_cast<int>(document["bufferViews"].size());
        document["bufferViews"].push_back({{"buffer",0},{"byteOffset",offset},{"byteLength",values.size()*4}});
        const int accessor = static_cast<int>(document["accessors"].size());
        document["accessors"].push_back({{"bufferView",view},{"componentType",5126},{"count",values.size()},{"type","SCALAR"}});
        return accessor;
    }
    std::string glb() const {
        auto source = document;
        source["buffers"][0]["byteLength"] = binary.size();
        std::string json = source.dump();
        while (json.size() % 4) json += ' ';
        std::string result;
        const auto append = [&](std::uint32_t word) {
            result.append(reinterpret_cast<const char*>(&word), 4);
        };
        append(0x46546c67); append(2);
        append(static_cast<std::uint32_t>(28 + json.size() + binary.size()));
        append(static_cast<std::uint32_t>(json.size())); append(0x4e4f534a);
        result += json;
        append(static_cast<std::uint32_t>(binary.size())); append(0x004e4942);
        result += binary;
        return result;
    }
};
void opacity(Fixture& fixture, std::size_t node, float value) {
    fixture.document["extensionsUsed"] = {"AZURE_node_opacity"};
    fixture.document["nodes"][node]["extensions"]["AZURE_node_opacity"] = {
        {"schemaVersion",1},{"opacity",value}
    };
}
Fixture animated(const std::vector<float>& values = {0,1}, const std::string& interpolation = "LINEAR",
                 const std::vector<float>& times = {0,2}) {
    Fixture fixture;
    opacity(fixture,0,.5F); opacity(fixture,1,.8F);
    const int input = fixture.scalars(times), output = fixture.scalars(values);
    fixture.document["animations"] = Json::array({{
        {"name","fade"}, {"channels",Json::array()},
        {"samplers",Json::array({{{"input",input},{"output",output},{"interpolation",interpolation}}})},
        {"extensions",{{"AZURE_node_opacity",{{"schemaVersion",1},
            {"channels",Json::array({{{"sampler",0},{"node",1}}})}}}}}
    }});
    return fixture;
}
void rejects(const Fixture& fixture, const char* message) {
    bool rejected = false;
    try { (void)loadEmbeddedGltfAsset(fixture.glb(), true); }
    catch (const std::exception&) { rejected = true; }
    check(rejected, message);
}
void near(float actual, float expected, const char* message) {
    check(std::isfinite(actual) && std::abs(actual - expected) < 1e-6F, message);
}
}

int main() {
    try {
        Fixture invalid;
        invalid.document["extensionsUsed"] = {"AZURE_node_opacity"};
        invalid.document["nodes"][0]["extensions"]["AZURE_node_opacity"] = {
            {"schemaVersion", 2}, {"opacity", .5}
        };
        bool rejected = false;
        try {
            (void)loadEmbeddedGltfAsset(invalid.glb(), true);
        } catch (const std::exception& error) {
            rejected = std::string(error.what()).find("AZURE_node_opacity") != std::string::npos;
        }
        check(rejected, "AZURE_node_opacity must reject an unsupported schema version");
        Fixture legacy;
        const auto oldAsset = loadEmbeddedGltfAsset(legacy.glb(), true);
        const auto oldPose = bindAnimationPose(oldAsset);
        check(oldPose.nodeWorldOpacities == std::vector<float>({1,1}),
              "Assets without the extension must retain complete opacity");
        check(oldAsset.primitives[0].nodeIndex == 1,
              "Imported primitives must preserve their owning mesh node");

        auto fixture = animated();
        const auto asset = loadEmbeddedGltfAsset(fixture.glb(), true);
        const auto bind = bindAnimationPose(asset);
        near(bind.nodeWorldOpacities[1], .4F, "Parent and child authored opacity must multiply");
        const auto middle = sampleAnimationPose(asset,0,1,false);
        near(middle.local[1].opacity,.5F,"Linear animation must replace the local node opacity");
        near(middle.nodeWorldOpacities[1],.25F,"Animated child opacity must inherit parent opacity");
        const auto first = sampleAnimationPose(asset,0,0,false);
        const auto last = sampleAnimationPose(asset,0,2,false);
        near(first.nodeWorldOpacities[1],0,"The first opacity key must apply");
        near(last.nodeWorldOpacities[1],.5F,"The last opacity key must apply");
        near(asset.nodes[1].opacity,.8F,"Sampling must leave the shared asset immutable");
        near(first.local[1].opacity,0,"Independent sampled instances must preserve their own age");
        near(blendAnimationPoses(asset,first,last,.25F).nodeWorldOpacities[1],.125F,
             "Pair blending must interpolate local opacity before ancestor multiplication");
        near(sampleAnimationBlend(asset,{{0,0,.25F,false,2},{0,2,.75F,false,2}}).nodeWorldOpacities[1],.375F,
             "Weighted clip blending must carry local opacity");
        auto fullOpacityFixture = animated({1,1});
        opacity(fullOpacityFixture,0,1);
        const auto fullOpacityAsset = loadEmbeddedGltfAsset(fullOpacityFixture.glb(),true);
        std::vector<azurerender::AnimationClipSample> tenSamples(10,{0,1,.1F,false,2});
        near(sampleAnimationBlend(fullOpacityAsset,tenSamples).nodeWorldOpacities[1],1,
             "Normalized blend accumulation must retain valid full opacity despite float rounding");
        check(azurerender::deformedVertexPosition(asset,1,&middle,{0,0}) == std::array<float,3>{1,0,0},
              "Opacity animation must preserve geometry rather than shrinking it");
        ExpressionPose neutral; neutral.id="neutral";
        neutral.skeletonFingerprint=expressionSkeletonFingerprint(asset);
        near(applyExpressionPose(asset,middle,neutral).nodeWorldOpacities[1],.25F,
             "Expression overlays must preserve node opacity");

        const auto step = loadEmbeddedGltfAsset(animated({0,1},"STEP").glb(),true);
        near(sampleAnimationPose(step,0,1,false).local[1].opacity,0,"STEP must hold the preceding opacity key");
        near(sampleAnimationPose(step,0,5,false).local[1].opacity,1,"Nonloop opacity must hold its last key");
        near(sampleAnimationPose(step,0,3,true).local[1].opacity,0,"Opacity loop time must follow the clip clock");
        const auto cubic = loadEmbeddedGltfAsset(animated({0,0,4,-4,1,0},"CUBICSPLINE").glb(),true);
        near(sampleAnimationPose(cubic,0,1,false).local[1].opacity,1,"Cubic opacity overshoot must clamp at one");
        const auto negativeCubic = loadEmbeddedGltfAsset(animated({0,1,-4,4,0,0},"CUBICSPLINE").glb(),true);
        near(sampleAnimationPose(negativeCubic,0,1,false).local[1].opacity,0,"Cubic opacity undershoot must clamp at zero");

        auto matrixFixture = animated();
        matrixFixture.document["nodes"][1]["matrix"]={1,0,0,0,0,1,0,0,0,0,1,0,4,5,0,1};
        const auto matrixAsset=loadEmbeddedGltfAsset(matrixFixture.glb(),true);
        const auto matrixPose=sampleAnimationPose(matrixAsset,0,1,false);
        near(matrixPose.nodeWorldOpacities[1],.25F,"Matrix-authored nodes must accept independent opacity");
        check(azurerender::deformedVertexPosition(matrixAsset,0,&matrixPose,{0,0})==std::array<float,3>{4,5,0},
              "Matrix placement must survive opacity animation");

        auto mixedFixture=animated();
        const int translation=mixedFixture.scalars({0,0,0,0,0,4});
        mixedFixture.document["accessors"][translation]["type"]="VEC3";
        mixedFixture.document["accessors"][translation]["count"]=2;
        mixedFixture.document["animations"][0]["samplers"].push_back(
            {{"input",1},{"output",translation},{"interpolation","LINEAR"}});
        mixedFixture.document["animations"][0]["channels"].push_back(
            {{"sampler",1},{"target",{{"node",1},{"path","translation"}}}});
        const auto mixedAsset=loadEmbeddedGltfAsset(mixedFixture.glb(),true);
        const auto mixedPose=sampleAnimationPose(mixedAsset,0,1,false);
        near(mixedPose.nodeWorldOpacities[1],.25F,
             "Opacity must compose with standard channels in the same animation");
        check(azurerender::deformedVertexPosition(mixedAsset,0,&mixedPose,{0,0})==std::array<float,3>{0,0,2},
              "Standard translation must remain active alongside opacity");

        // A grouped sign uses the same scalar sampler for its parent and child.
        auto sign = animated();
        sign.document["nodes"][0]["name"]="prompt-group";
        sign.document["nodes"][1]["name"]="sign";
        sign.document["animations"][0]["extensions"]["AZURE_node_opacity"]["channels"].push_back({{"sampler",0},{"node",0}});
        const auto signAsset=loadEmbeddedGltfAsset(sign.glb(),true);
        near(sampleAnimationPose(signAsset,0,1,false).nodeWorldOpacities[1],.25F,
             "A parent and child may share one sampler while retaining separate local targets");

        for (const Json value : {Json(-.1),Json(1.1),Json("half"),Json(true)}) {
            auto bad=fixture;bad.document["nodes"][0]["extensions"]["AZURE_node_opacity"]["opacity"]=value;
            rejects(bad,"Invalid authored opacity must reject");
        }
        for(int kind=0;kind<11;++kind){
            auto bad=fixture;
            auto& extension=bad.document["animations"][0]["extensions"]["AZURE_node_opacity"];
            auto& channel=extension["channels"][0];
            if(kind==0)extension["schemaVersion"]=2;
            if(kind==1)extension.erase("schemaVersion");
            if(kind==2)channel["node"]=2;
            if(kind==3)channel["sampler"]=1;
            if(kind==4)channel["node"]=-1;
            if(kind==5)channel["sampler"]=.5;
            if(kind==6)extension["channels"].push_back(channel);
            if(kind==7)extension["unexpected"]=1;
            if(kind==8)bad.document["extensionsUsed"]=Json::array();
            if(kind==9)channel["node"]=true;
            if(kind==10)extension["channels"]=Json::object();
            rejects(bad,"Malformed, undeclared or duplicate opacity animation must reject");
        }
        rejects(animated({-.1F,1}),"Opacity animation keys below zero must reject");
        rejects(animated({0,1.1F}),"Opacity animation keys above one must reject");
        rejects(animated({0,std::numeric_limits<float>::quiet_NaN()}),"Nonfinite opacity keys must reject");
        rejects(animated({0,1},"CATMULLROM"),"Unknown opacity interpolation must reject");
        rejects(animated({0,1},"LINEAR",{0,0}),"Repeated opacity key times must reject");
        rejects(animated({0},"LINEAR"),"Opacity output key count mismatch must reject");
        for (int kind=0;kind<4;++kind) {
            auto bad=fixture;
            const auto& sampler=bad.document["animations"][0]["samplers"][0];
            const int accessor=sampler[kind<2?"input":"output"].get<int>();
            if(kind%2==0)bad.document["accessors"][accessor]["componentType"]=5123;
            else bad.document["accessors"][accessor]["type"]="VEC2";
            rejects(bad,"Opacity animation accessors must use FLOAT SCALAR values");
        }
        auto invalidPose=middle;invalidPose.local[1].opacity=-.1F;
        rejected=false;try{resolveAssetPose(asset,invalidPose);}catch(const std::exception&){rejected=true;}
        check(rejected,"Externally supplied invalid pose opacity must reject");
        std::cout << "Node opacity defaults, hierarchy, sampling, blending, expressions and validation passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
