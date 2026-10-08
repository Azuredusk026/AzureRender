#include "assets/GltfLoader.hpp"
#include "runtime/LevelRenderSettings.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <cmath>
#include <chrono>
using namespace azurerender;
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
int main(int argc,char** argv) { try {
    require(argc==2,"Public material fixture path is required");
    nlohmann::json input;std::ifstream(argv[1])>>input;
    auto& material=input["materials"][0];
    material["pbrMetallicRoughness"]={{"baseColorFactor",{.5,.25,1.,1.}}, {"roughnessFactor",.25},{"metallicFactor",.6}};
    const auto sourceDirectory=std::filesystem::absolute(argv[1]).parent_path();
    auto directory=std::filesystem::temp_directory_path()/("azure-surface-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(directory);
    struct Cleanup{std::filesystem::path path;~Cleanup(){std::error_code error;std::filesystem::remove_all(path,error);}}cleanup{directory};
    unsigned externalFile=0;
    for (const char* section:{"buffers","images"}) for (auto& entry:input[section]) {
        if(entry.contains("uri")) {auto uri=entry["uri"].get<std::string>();
            if(uri.rfind("data:",0)!=0) {
                const auto source=std::filesystem::path(uri).is_relative()?sourceDirectory/uri:std::filesystem::path(uri);
                const auto name="external-"+std::to_string(externalFile++)+source.extension().string();
                std::filesystem::copy_file(source,directory/name);entry["uri"]=name;
            }}
    }
    auto path=directory/"fixture.gltf";
    material["extras"]["azureRenderMaterial"]={{"schemaVersion",2},{"class","hair"},{"features",{"hair-anisotropy"}},
        {"hair",{{"power",200},{"strength",.2},{"rampRow",6},{"strandShift",.07},{"cameraOffset",{0,0,.6}},{"upperLimit",3}}}};
    std::ofstream(path)<<input.dump();
    const auto asset=loadGltfAsset(path.string());const auto& m=asset.materials.at(0);
    require(std::abs(int(m.baseColorPixels[0])-188)<=1,"Linear baseColorFactor must be encoded for sRGB texture storage");
    require(std::abs(int(m.metallicRoughnessPixels[1])-64)<=1,"Untextured roughnessFactor must be preserved");
    require(std::abs(int(m.metallicRoughnessPixels[2])-153)<=1,"Untextured metallicFactor must be preserved");
    require(std::abs(m.hairParameters[2]-6)<.001F,"Hair ramp row must retain its semantic field");
    require(std::abs(m.hairParameters[3]-.07F)<.001F,"Strand shift must be independent from ramp row");
    material["extras"]["azureRenderMaterial"]["hair"]["rampTexture"]=.5;
    material["extras"]["azureRenderMaterial"]["features"].push_back("hair-ramp");
    std::ofstream(path)<<input.dump();bool invalidRampRejected=false;
    try{(void)loadGltfAsset(path.string());}catch(const std::exception&){invalidRampRejected=true;}
    require(invalidRampRejected,"Fractional hair ramp texture index must be rejected");
    RenderSettings settings;decodeLevelRenderSettings(settings,{{"antiAliasing",2}});
    require(encodeLevelRenderSettings(settings).value("antiAliasing",-1)==2,"Anti-aliasing quality must round trip through production level settings");
    bool rejected=false;try{decodeLevelRenderSettings(settings,{{"antiAliasing",7}});}catch(const std::exception&){rejected=true;}
    require(rejected,"Unknown anti-aliasing quality must be rejected");
    std::cout<<"Surface factors, linear color and anti-aliasing settings passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
