#include "assets/GltfLoader.hpp"
#include "assets/TextureImportSettings.hpp"
#include "resources/ResourceLocator.hpp"
#include <stb_image_write.h>
#include <webp/encode.h>
#include <chrono>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
using namespace azurerender;
int main(){const auto root=std::filesystem::temp_directory_path()/("azure-texture-import-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));try{
 std::filesystem::create_directories(root);std::ifstream input(ResourceLocator().publicAsset("test_model.gltf"));nlohmann::json source;input>>source;
 const int texture=source.at("materials")[0].at("normalTexture").at("index").get<int>();const int image=source.at("textures")[texture].at("source").get<int>();
 source["images"][image]["uri"]="normal.png";
 const unsigned char pixels[]={128,128,255,255,128,128,255,255,128,128,255,255,128,128,255,255};
 if(!stbi_write_png((root/"normal.png").string().c_str(),2,2,4,pixels,8))throw std::runtime_error("Cannot create image fixture");
 std::ofstream(root/"model.gltf")<<source.dump();
 const auto settings=TextureImportSettings::parse({{"usage","normal"},{"wrapV","clamp"},{"filter","nearest"},{"mipPolicy","none"}});
 std::ofstream(root/"normal.png.azmeta")<<nlohmann::json{{"schemaVersion",1},{"id","00000000-0000-4000-8000-000000000001"},{"settings",{{"texture",{{"filter","nearest"}}}}},{"dependencies",nlohmann::json::array()}}.dump();
 const auto inherited=loadGltfAsset((root/"model.gltf").string());
 if(inherited.materials[0].textureSettings[1].usage!="normal"||inherited.materials[0].textureSettings[1].colorSpace!="linear")throw std::runtime_error("Sampler-only settings must preserve the normal slot's semantic defaults");
 std::ofstream(root/"normal.png.azmeta")<<nlohmann::json{{"schemaVersion",1},{"id","00000000-0000-4000-8000-000000000001"},{"settings",{{"texture",settings.describe()}}},{"dependencies",nlohmann::json::array()}}.dump();
 const auto asset=loadGltfAsset((root/"model.gltf").string());const auto& actual=asset.materials[0].textureSettings[1];
 if(actual.describe()!=settings.describe())throw std::runtime_error("Image import settings must reach the normal material slot");
 if(actual.semantic()!=TextureFilterSemantic::Normal||actual.sampler(1,8).addressV!=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE)throw std::runtime_error("Material sampling must consume normal settings");
 unsigned char* webp=nullptr;const auto webpSize=WebPEncodeLosslessRGBA(pixels,2,2,8,&webp);
 if(!webpSize)throw std::runtime_error("Cannot create WebP material fixture");
 {std::ofstream file(root/"normal.webp",std::ios::binary);file.write(reinterpret_cast<const char*>(webp),webpSize);}WebPFree(webp);
 source["images"][image]["uri"]="normal.webp";std::ofstream(root/"webp.gltf")<<source.dump();
 const auto webpAsset=loadGltfAsset((root/"webp.gltf").u8string());
 if(webpAsset.materials[0].normalPixels!=asset.materials[0].normalPixels)throw std::runtime_error("External WebP must reach the same production normal texture pixels");
 const float linearNormal[]={.5F,.5F,1.F,.5F,.5F,1.F,.5F,.5F,1.F,.5F,.5F,1.F};
 if(!stbi_write_hdr((root/"normal.hdr").string().c_str(),2,2,3,linearNormal))throw std::runtime_error("Cannot create linear normal fixture");
 source["images"][image]["uri"]="normal.hdr";std::ofstream(root/"linear.gltf")<<source.dump();
 const auto linearAsset=loadGltfAsset((root/"linear.gltf").u8string());
 if(linearAsset.materials[0].normalPixels[0]!=128||linearAsset.materials[0].normalPixels[1]!=128)throw std::runtime_error("Linear floating-point normal data must avoid color gamma encoding");
 source["materials"][0]["alphaMode"]="BLEND";
 source["materials"][0]["extras"]["azureRenderMaterial"]={{"schemaVersion",2},{"class","generic"},{"features",{"scene-refraction"}},
     {"sceneRefraction",{{"texture",texture},{"uvTransform",{.7,.7,0,0}},{"tint",{.8,.9,1,1}},{"contribution",.8},{"brightness",.271},{"distortionStrength",.008}}}};
 std::ofstream(root/"glass.gltf")<<source.dump();
 const auto glass=loadGltfAsset((root/"glass.gltf").u8string());const auto& g=glass.materials[0];
 if(!(g.materialFeatures&1024U)||g.styleMaskPixels!=linearAsset.materials[0].normalPixels
     ||std::abs(g.hairParameters[0]-.8F)>1e-6F||std::abs(g.hairParameters[1]-.271F)>1e-6F||g.styleParameters[0]!=.7F)
     throw std::runtime_error("Named scene refraction must preserve linear distortion pixels and typed parameters");
 for(const auto bad:nlohmann::json::array({-1,2,"wrong"})) {
     auto invalid=source;invalid["materials"][0]["extras"]["azureRenderMaterial"]["sceneRefraction"]["contribution"]=bad;
     std::ofstream(root/"invalid-glass.gltf")<<invalid.dump();bool rejected=false;
     try{(void)loadGltfAsset((root/"invalid-glass.gltf").u8string());}catch(const std::exception&){rejected=true;}
     if(!rejected)throw std::runtime_error("Invalid refraction contribution must be rejected");
 }
 auto missing=source;missing["textures"][texture]["source"]=-1;
 std::ofstream(root/"missing-glass.gltf")<<missing.dump();bool missingRejected=false;
 try{(void)loadGltfAsset((root/"missing-glass.gltf").u8string());}catch(const std::exception&){missingRejected=true;}
 if(!missingRejected)throw std::runtime_error("Required refraction image cannot silently use a fallback");
 source["materials"][0]["alphaMode"]="OPAQUE";std::ofstream(root/"opaque-glass.gltf")<<source.dump();bool rejected=false;
 try{(void)loadGltfAsset((root/"opaque-glass.gltf").u8string());}catch(const std::exception&){rejected=true;}
 if(!rejected)throw std::runtime_error("Scene refraction requires the transparent queue");
 std::filesystem::remove_all(root);return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';std::filesystem::remove_all(root);return 1;}}
