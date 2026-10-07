#include "assets/generators/RigTextContracts.hpp"
#include "assets/GltfLoader.hpp"
#include "runtime/AnimationStateMachine.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace azurerender;
using Json=nlohmann::json;
void require(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
template<class F>void reject(F f){bool failed=false;try{f();}catch(const std::exception&){failed=true;}require(failed,"Invalid rigid animation must be rejected");}
int main(int argc,char** argv){try{
    require(argc==2,"Expected output directory");
    Json source={{"schemaVersion",1},{"coordinates","engine"},{"bones",Json::array({
        {{"name","bone_root"},{"translation",{0,0,0}},{"geometry",{{"op","cube"},{"size",{1,1,1}}}}},
        {{"name","bone_arm"},{"parent","bone_root"},{"translation",{0,1,0}},{"geometry",{{"op","cube"},{"size",{1,.25,.25}}}}}})},
        {"animations",Json::array({
            {{"name","anim_wave"},{"loop",true},{"channels",Json::array({{{"bone","bone_arm"},{"path","pos"},{"keys",{{0,{0,0,0}},{1,{2,0,0}}}}}})}},
            {{"name","anim_reach"},{"loop",false},{"channels",Json::array({
                {{"bone","bone_arm"},{"path","rot"},{"keys",{{0,{0,0,0}},{1,{0,0,90}}}}},
                {{"bone","bone_root"},{"path","scale"},{"keys",{{0,{1,1,1}},{1,{2,2,2}}}}}})}}})}};
    const auto generate=[&](Json value,double scale=1){RigTextRequest r;r.geometry.source=value.dump();r.scale=scale;return generateRigText(r);};
    const auto rig=generate(source);const auto path=std::filesystem::path(argv[1])/"rig.gltf";std::filesystem::create_directories(path.parent_path());std::ofstream(path)<<rig.gltf;
    const auto asset=loadGltfAsset(path.string());require(asset.hasSkin && asset.jointNodes.size()==2 && asset.animations.size()==2,"Text parts become an imported two-bone animated asset");
    for(const auto& vertex:asset.vertices)require(vertex.weights==std::array<float,4>{1,0,0,0},"Each rigid vertex has one bone owner");
    const auto bind=bindAnimationPose(asset),early=sampleAnimationPose(asset,0,.25F),late=sampleAnimationPose(asset,0,.75F);
    require(bind.local[asset.jointNodes[1]].translation==std::array<float,3>{0,1,0},"Parent-local pivot is preserved");
    require(std::abs(early.local[asset.jointNodes[1]].translation[0]-.5F)<1e-5 && std::abs(late.local[asset.jointNodes[1]].translation[0]-1.5F)<1e-5,"Instances sample independent animation times");
    auto graph=AnimationStateMachine::parse(rig.animationGraph);require(graph.loop(),"Loop metadata enters the runtime graph");graph.select("anim_reach");require(!graph.loop(),"One-shot metadata enters the runtime graph");
    const auto finished=sampleAnimationPose(asset,graph.clip(),2,false);require(finished.local[asset.jointNodes[0]].scale==std::array<float,3>{2,2,2},"Non-loop scale channels hold the final key");
    require(blendAnimationPoses(asset,early,late,.5F).local[asset.jointNodes[1]].translation[0]==1,"Existing crossfade consumes rigid animation poses");
    auto scad=source;scad["coordinates"]="scad";std::ofstream(path)<<generate(scad,2).gltf;const auto converted=loadGltfAsset(path.string());
    require(bindAnimationPose(converted).local[converted.jointNodes[1]].translation==std::array<float,3>{0,0,-2},"Rig pivots share geometry coordinate conversion and scale");
    for(const char* issue:{"unknown","cycle","time","scale","duplicate"}){
        auto invalid=source;
        if(std::string(issue)=="unknown")invalid["animations"][0]["channels"][0]["bone"]="bone_missing";
        if(std::string(issue)=="cycle")invalid["bones"][0]["parent"]="bone_arm";
        if(std::string(issue)=="time")invalid["animations"][0]["channels"][0]["keys"][1][0]=0;
        if(std::string(issue)=="scale")invalid["animations"][1]["channels"][1]["keys"][1][1][0]=0;
        if(std::string(issue)=="duplicate")invalid["bones"][1]["name"]="bone_root";
        reject([&]{generate(invalid);});
    }
    reject([&]{generate(source,0);});reject([&]{generate(source,-1);});
    require(generate(source).gltf==rig.gltf,"Repeated rig generation is byte deterministic");
    std::cout<<"Rigid hierarchy, ownership, coordinates, channels, loops, independent poses and failures passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
