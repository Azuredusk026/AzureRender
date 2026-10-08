#include "assets/GltfLoader.hpp"
#include "runtime/LevelRenderSettings.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <cmath>
using namespace azurerender;
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
int main(int argc,char** argv) { try {
    require(argc==2,"Public material fixture path is required");
    nlohmann::json input;std::ifstream(argv[1])>>input;
    auto& material=input["materials"][0];
    material["pbrMetallicRoughness"]={{"baseColorFactor",{.5,.25,1.,1.}}, {"roughnessFactor",.25},{"metallicFactor",.6}};
    auto path=std::filesystem::path(argv[1]);path.replace_filename("surface-factor-test.gltf");
    std::ofstream(path)<<input.dump();
    const auto asset=loadGltfAsset(path.string());const auto& m=asset.materials.at(0);
    require(std::abs(int(m.baseColorPixels[0])-188)<=1,"Linear baseColorFactor must be encoded for sRGB texture storage");
    require(std::abs(int(m.metallicRoughnessPixels[1])-64)<=1,"Untextured roughnessFactor must be preserved");
    require(std::abs(int(m.metallicRoughnessPixels[2])-153)<=1,"Untextured metallicFactor must be preserved");
    RenderSettings settings;decodeLevelRenderSettings(settings,{{"antiAliasing",2}});
    require(encodeLevelRenderSettings(settings).value("antiAliasing",-1)==2,"Anti-aliasing quality must round trip through production level settings");
    bool rejected=false;try{decodeLevelRenderSettings(settings,{{"antiAliasing",7}});}catch(const std::exception&){rejected=true;}
    require(rejected,"Unknown anti-aliasing quality must be rejected");
    std::cout<<"Surface factors, linear color and anti-aliasing settings passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
