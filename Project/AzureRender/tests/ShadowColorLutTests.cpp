#include "assets/GltfLoader.hpp"
#include "assets/MaterialSurfaceProfile.hpp"
#include "resources/ResourceLocator.hpp"
#include <stb_image_write.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

using Json=nlohmann::json;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
}
int main(){
 const auto root=std::filesystem::temp_directory_path()/("azure-shadow-lut-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 try {
  std::filesystem::create_directories(root);
  Json source;std::ifstream(azurerender::ResourceLocator().publicAsset("test_model.gltf"))>>source;
  // A 2-cube strip has two red texels per blue slice and two green rows.
  const unsigned char pixels[]={0,0,0,255,255,0,0,255,0,0,255,255,255,0,255,255,
                                0,255,0,255,255,255,0,255,0,255,255,255,255,255,255,255};
  require(stbi_write_png((root/"lut.png").string().c_str(),4,2,4,pixels,16)!=0,"Cannot create LUT fixture");
  source["images"].push_back({{"uri","lut.png"}});
  const auto index=source["textures"].size();
  source["textures"].push_back({{"source",source["images"].size()-1}});
  source["materials"][0]["extras"]["azureRenderMaterial"]={{"schemaVersion",2},{"class","generic"},{"features",Json::array()},
    {"shadowColorLut",{{"schemaVersion",1},{"texture",index},{"flipV",false}}}};
  auto load=[&](const Json& document){std::ofstream(root/"model.gltf")<<document.dump();return loadGltfAsset((root/"model.gltf").string());};
  const auto asset=load(source);
  require((asset.materials[0].materialFeatures&4096U)!=0,"Explicit shadow LUT must reach runtime material features");
  require(asset.materials[0].textureSettings.size()==9,"Shadow LUT requires an independent texture slot");
  require(asset.materials[0].shadowColorLutPixels==std::vector<std::uint8_t>(std::begin(pixels),std::end(pixels)),"LUT bytes must preserve source sRGB encoding");
  auto flipped=source;flipped["materials"][0]["extras"]["azureRenderMaterial"]["shadowColorLut"]["flipV"]=true;
  const auto inverted=load(flipped);
  require(inverted.materials[0].shadowColorLutPixels[1]==255&&inverted.materials[0].shadowColorLutPixels[17]==0,"Explicit source row flipping must preserve cube orientation");
  const auto& settings=asset.materials[0].textureSettings.at(8);
  require(settings.colorSpace=="srgb"&&settings.filter=="linear"&&settings.mipPolicy=="none"&&settings.wrapU=="clamp"&&settings.wrapV=="clamp","LUT sampling must decode sRGB before interpolation at LOD zero with edge clamping");
  const auto baseline=source;
  auto composed=source;
  auto& npr=composed["materials"][0]["extras"]["azureRenderMaterial"]["nprDiffuse"];
  npr={{"schemaVersion",1},{"shadowScale",.7},{"lightingInputs","host-scene-radiance"},
    {"shadowMask","none"},{"baseAo","albedo-alpha"}};
  const auto response=load(composed);
  require((response.materials[0].materialFeatures&(1U<<19U))!=0,"NPR color composition must be explicitly selected");
  const auto packed=azurerender::packMaterialSurfaceProfile(response.materials[0]);
  require(std::abs(packed.rows[0][0]-.7F)<1e-6F&&packed.rows[0][1]==0&&packed.rows[0][2]==1,
    "NPR scale and source selectors must reach the immutable GPU table");
  for(const char* kind:{"generic","skin","face","fabric","metal"}) {
    auto valid=composed;valid["materials"][0]["extras"]["azureRenderMaterial"]["class"]=kind;
    require((load(valid).materials[0].materialFeatures&(1U<<19U))!=0,"NPR composition must support declared surface classes");
  }
  auto mask=composed;mask["materials"][0]["extras"]["azureRenderMaterial"]["nprDiffuse"]["shadowMask"]="style-mask-gb";
  require(azurerender::packMaterialSurfaceProfile(load(mask).materials[0]).rows[0][1]==1,"Source mask selector must survive packing");
  for(const auto& bad:std::vector<Json>{nullptr,Json::array(),Json{{"schemaVersion",1}},
    Json{{"schemaVersion",2},{"shadowScale",1},{"lightingInputs","host-scene-radiance"},{"shadowMask","none"},{"baseAo","one"}}}) {
    auto invalid=composed;invalid["materials"][0]["extras"]["azureRenderMaterial"]["nprDiffuse"]=bad;
    bool rejected=false;try{load(invalid);}catch(const std::exception&){rejected=true;}
    require(rejected,"Incomplete or unsupported NPR composition must fail import");
  }
  for(const auto& field:std::vector<std::pair<std::string,Json>>{{"schemaVersion",1.5},{"schemaVersion",true},
    {"shadowScale",-1},{"shadowScale",65},{"shadowScale",nullptr},{"shadowScale",true},
    {"lightingInputs","native-unverified"},{"shadowMask","unknown"},{"baseAo","packed-ao"},{"unknown",1}}) {
    auto invalid=composed;invalid["materials"][0]["extras"]["azureRenderMaterial"]["nprDiffuse"][field.first]=field.second;
    bool rejected=false;try{load(invalid);}catch(const std::exception&){rejected=true;}
    require(rejected,"Malformed NPR composition fields must fail import");
  }
  for(const char* kind:{"hair","eye","overlay","emissive","showcase"}) {
    auto invalid=composed;invalid["materials"][0]["extras"]["azureRenderMaterial"]["class"]=kind;
    bool rejected=false;try{load(invalid);}catch(const std::exception&){rejected=true;}
    require(rejected,"Incompatible material table owners must fail import");
  }
  auto noLut=composed;noLut["materials"][0]["extras"]["azureRenderMaterial"].erase("shadowColorLut");
  bool missingRejected=false;try{load(noLut);}catch(const std::exception&){missingRejected=true;}
  require(missingRejected,"NPR composition requires an explicit shadow LUT");
  for(const auto& schema:std::vector<Json>{1,2.5,"2",nullptr}) {
    auto invalid=composed;invalid["materials"][0]["extras"]["azureRenderMaterial"]["schemaVersion"]=schema;
    bool rejected=false;try{load(invalid);}catch(const std::exception&){rejected=true;}
    require(rejected,"NPR composition requires integer parent schema two");
  }
  for(const auto& bad:std::vector<Json>{Json{{"schemaVersion",2},{"texture",index}},Json{{"schemaVersion",1.5},{"texture",index}},
     Json{{"schemaVersion",1},{"texture",-1}},Json{{"schemaVersion",1},{"texture",.5}},
     Json{{"schemaVersion",1},{"texture",index},{"flipV",1}},Json{{"schemaVersion",1},{"texture",index},{"unknown",true}}}){
    auto invalid=baseline;invalid["materials"][0]["extras"]["azureRenderMaterial"]["shadowColorLut"]=bad;
    bool rejected=false;try{load(invalid);}catch(const std::exception&){rejected=true;}
    require(rejected,"Malformed shadow LUT configuration must fail import");
  }
  require(stbi_write_png((root/"lut.png").string().c_str(),2,2,4,pixels,8)!=0,"Cannot create invalid LUT fixture");
  bool rejected=false;try{load(baseline);}catch(const std::exception&){rejected=true;}
  require(rejected,"LUT strip dimensions must encode a complete cube");
  std::filesystem::remove_all(root);
  std::cout<<"Shadow LUT import, sampling semantics and invalid inputs passed\n";return 0;
 }catch(const std::exception& error){std::cerr<<error.what()<<'\n';std::filesystem::remove_all(root);return 1;}
}
