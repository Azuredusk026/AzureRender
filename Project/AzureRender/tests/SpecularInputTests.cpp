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
namespace {void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}}
int main(){
 const auto root=std::filesystem::temp_directory_path()/("azure-specular-input-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 try {
  std::filesystem::create_directories(root);
  Json source;std::ifstream(azurerender::ResourceLocator().publicAsset("test_model.gltf"))>>source;
  const unsigned char pixels[]={123,178,19,37};
  require(stbi_write_png((root/"packed.png").string().c_str(),1,1,4,pixels,4)!=0,"Cannot write fixture");
  const auto image=source["images"].size();source["images"].push_back({{"uri","packed.png"}});
  const auto texture=source["textures"].size();source["textures"].push_back({{"source",image}});
  auto& pbr=source["materials"][0]["pbrMetallicRoughness"];
  pbr["metallicRoughnessTexture"]={{"index",texture}};pbr["metallicFactor"]=1;pbr["roughnessFactor"]=1;
  source["materials"][0]["extras"]["azureRenderMaterial"]={{"schemaVersion",2},{"class","fabric"},{"features",Json::array()},
    {"specularInput",{{"schemaVersion",1},{"source","metallic-roughness-alpha"},{"scale",.5}}}};
  auto load=[&](const Json& doc){std::ofstream(root/"model.gltf")<<doc.dump();return loadGltfAsset((root/"model.gltf").string());};
  for(const char* kind:{"generic","fabric","metal"}) {
   auto valid=source;valid["materials"][0]["extras"]["azureRenderMaterial"]["class"]=kind;
   const auto result=load(valid);const auto packed=azurerender::packMaterialSurfaceProfile(result.materials[0]);
   require(packed.rows[5][2]==1.F&&packed.rows[5][3]==.5F,"Explicit ORM specular input must reach GPU table");
   require(result.materials[0].metallicRoughnessPixels==std::vector<std::uint8_t>(std::begin(pixels),std::end(pixels)),"ORM alpha must preserve source bytes");
  }
  auto explicitEmissive=source;explicitEmissive["materials"][0]["extras"]["azureRenderMaterial"]["specularInput"]["source"]="emissive-alpha";
  auto e=azurerender::packMaterialSurfaceProfile(load(explicitEmissive).materials[0]);
  require(e.rows[5][2]==0&&e.rows[5][3]==.5F,"Emissive source must retain explicit scale");
  auto legacy=source;legacy["materials"][0]["extras"]["azureRenderMaterial"].erase("specularInput");
  auto l=azurerender::packMaterialSurfaceProfile(load(legacy).materials[0]);
  require(l.rows[5][2]==0&&l.rows[5][3]==1,"Omitted input must preserve emissive-alpha response");
  auto rejects=[&](const Json& doc){bool rejected=false;try{load(doc);}catch(const std::exception&){rejected=true;}require(rejected,"Malformed specular input must fail import");};
  for(const Json& value:std::vector<Json>{nullptr,true,0,"wrong",Json::array(),Json::object()}){
   auto bad=source;bad["materials"][0]["extras"]["azureRenderMaterial"]["specularInput"]=value;rejects(bad);
  }
  for(const Json& value:std::vector<Json>{-1,1.1,nullptr,true,"0.5"}){
   auto bad=source;bad["materials"][0]["extras"]["azureRenderMaterial"]["specularInput"]["scale"]=value;rejects(bad);
  }
  for(const char* field:{"schemaVersion","source","scale"}){
   auto bad=source;bad["materials"][0]["extras"]["azureRenderMaterial"]["specularInput"].erase(field);rejects(bad);
  }
  for(const char* kind:{"skin","face","hair","eye","overlay","emissive","showcase"}){
   auto bad=source;bad["materials"][0]["extras"]["azureRenderMaterial"]["class"]=kind;rejects(bad);
  }
  auto missing=source;missing["materials"][0]["pbrMetallicRoughness"].erase("metallicRoughnessTexture");rejects(missing);
  auto extra=source;extra["materials"][0]["extras"]["azureRenderMaterial"]["specularInput"]["extra"]=0;rejects(extra);
  auto version=source;version["materials"][0]["extras"]["azureRenderMaterial"]["specularInput"]["schemaVersion"]=2;rejects(version);
  auto unknown=source;unknown["materials"][0]["extras"]["azureRenderMaterial"]["specularInput"]["source"]="unknown";rejects(unknown);
  std::filesystem::remove_all(root);std::cout<<"Specular input loading and packing passed\n";return 0;
 } catch(const std::exception& error){std::cerr<<error.what()<<'\n';std::filesystem::remove_all(root);return 1;}
}
