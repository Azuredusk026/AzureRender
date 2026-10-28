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
 // Material factors use nearest quantization on both image and constant paths.
 const unsigned char whitePixels[]={255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255};
 if(!stbi_write_png((root/"white.png").string().c_str(),2,2,4,whitePixels,8))throw std::runtime_error("Cannot create white factor fixture");
 auto factorSource=source;
 const auto whiteImage=factorSource["images"].size();factorSource["images"].push_back({{"uri","white.png"}});
 const auto whiteTexture=factorSource["textures"].size();factorSource["textures"].push_back({{"source",whiteImage}});
 auto& factorPbr=factorSource["materials"][0]["pbrMetallicRoughness"];factorPbr.erase("baseColorTexture");
 const auto checkFactor=[&](const nlohmann::json& factor,const std::array<unsigned char,4>& expected){
     factorPbr["baseColorFactor"]=factor;factorPbr.erase("baseColorTexture");
     std::ofstream(root/"constant-factor.gltf")<<factorSource.dump();
     const auto constant=loadGltfAsset((root/"constant-factor.gltf").string());
     for(std::size_t pixel=0;pixel<constant.materials[0].baseColorPixels.size();++pixel)
         if(constant.materials[0].baseColorPixels[pixel]!=expected[pixel%4])
             throw std::runtime_error("Untextured linear baseColorFactor must use nearest sRGB/linear-alpha byte quantization");
     factorPbr["baseColorTexture"]={{"index",whiteTexture}};
     std::ofstream(root/"white-texture-factor.gltf")<<factorSource.dump();
     const auto textured=loadGltfAsset((root/"white-texture-factor.gltf").string());
     if(textured.materials[0].baseColorPixels!=constant.materials[0].baseColorPixels)
         throw std::runtime_error("White texture and untextured baseColorFactor must share quantization semantics");
 };
 auto defaultWhite=factorSource;defaultWhite["materials"][0]["pbrMetallicRoughness"].erase("baseColorFactor");
 std::ofstream(root/"default-white.gltf")<<defaultWhite.dump();
 if(loadGltfAsset((root/"default-white.gltf").string()).materials[0].baseColorPixels!=std::vector<std::uint8_t>(16,255))
     throw std::runtime_error("Untextured default-white material must preserve all four channels at byte 255");
 checkFactor({.0005,.003,.01,.5},{2,10,25,128});
 checkFactor({1,1,1,1},{255,255,255,255});
 checkFactor({.18,.04,.7,1},{118,56,218,255});
 checkFactor({0,0,0,0},{0,0,0,0});
 // Standard samplers remain per texture binding and precede explicit image overrides.
 auto samplerSource=source;
 samplerSource["materials"][0]["name"]="GenericSamplerFixture";
 for(int minFilter:{9728,9729,9984,9985,9986,9987})for(int magFilter:{9728,9729}) {
  samplerSource["samplers"]=nlohmann::json::array({{{"minFilter",minFilter},{"magFilter",magFilter},{"wrapS",33071},{"wrapT",33648}}});
  samplerSource["textures"][texture]["sampler"]=0;std::ofstream(root/"sampler.gltf")<<samplerSource.dump();
  const auto imported=loadGltfAsset((root/"sampler.gltf").string());const auto& declared=imported.materials[0].textureSettings[1];const auto actualSampler=declared.sampler(4,16);
  const bool nearest=minFilter==9728||minFilter==9984||minFilter==9986;
  if(actualSampler.filter!=(nearest?VK_FILTER_NEAREST:VK_FILTER_LINEAR)
      ||actualSampler.effectiveMagFilter()!=(magFilter==9728?VK_FILTER_NEAREST:VK_FILTER_LINEAR)
      ||actualSampler.mipmapLinear!=(minFilter==9986||minFilter==9987)
      ||actualSampler.addressU!=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE||actualSampler.addressV!=VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT
      ||(declared.mipPolicy=="none")!=(minFilter==9728||minFilter==9729))throw std::runtime_error("glTF sampler filtering and wrapping must reach ordinary material slots");
 }
 for(int invalidSampler:{-2,2}) {
  auto invalid=samplerSource;invalid["textures"][texture]["sampler"]=invalidSampler;std::ofstream(root/"invalid-sampler.gltf")<<invalid.dump();
  bool rejected=false;try{(void)loadGltfAsset((root/"invalid-sampler.gltf").string());}catch(const std::exception&){rejected=true;}
  if(!rejected)throw std::runtime_error("Invalid glTF sampler references must fail");
 }
 auto absent=samplerSource;absent["textures"][texture].erase("sampler");
 std::ofstream(root/"absent-sampler.gltf")<<absent.dump();
 const auto absentMaterial=loadGltfAsset((root/"absent-sampler.gltf").string());
 if(absentMaterial.materials[0].textureSettings[1].describe()!=AssetMaterial{}.textureSettings[1].describe())
  throw std::runtime_error("Absent glTF sampler must retain material defaults");
 auto split=samplerSource;split["samplers"]=nlohmann::json::array({{{"minFilter",9985},{"magFilter",9728}},{{"minFilter",9986},{"magFilter",9729}}});
 split["materials"].push_back(split["materials"][0]);
 const auto secondTexture=split["textures"].size();split["textures"].push_back({{"source",image},{"sampler",1}});
 split["materials"][1]["normalTexture"]["index"]=secondTexture;
 std::ofstream(root/"split-samplers.gltf")<<split.dump();
 const auto splitAsset=loadGltfAsset((root/"split-samplers.gltf").string());
 if(splitAsset.materials[0].normalPixels!=splitAsset.materials[1].normalPixels
     ||splitAsset.materials[0].textureSettings[1].sampler(4,16).effectiveMagFilter()!=VK_FILTER_NEAREST
     ||splitAsset.materials[1].textureSettings[1].sampler(4,16).effectiveMagFilter()!=VK_FILTER_LINEAR)
  throw std::runtime_error("Shared glTF image must retain per-binding samplers");
 std::ofstream(root/"normal.png.azmeta")<<nlohmann::json{{"schemaVersion",1},{"id","00000000-0000-4000-8000-000000000001"},{"settings",{{"texture",{{"mipBias",-.5}}}}},{"dependencies",nlohmann::json::array()}}.dump();
 const auto partial=loadGltfAsset((root/"split-samplers.gltf").string());
 if(partial.materials[0].textureSettings[1].mipBias!=-.5F
     ||partial.materials[0].textureSettings[1].sampler(4,16).effectiveMagFilter()!=VK_FILTER_NEAREST
     ||partial.materials[1].textureSettings[1].sampler(4,16).effectiveMagFilter()!=VK_FILTER_LINEAR)
  throw std::runtime_error("Partial image metadata must retain undeclared glTF sampler fields");
 std::filesystem::remove(root/"normal.png.azmeta");
 for(const char* field:{"minFilter","magFilter","wrapS"}) {
  auto invalid=samplerSource;invalid["samplers"][0][field]=12345;std::ofstream(root/"invalid-filter.gltf")<<invalid.dump();
  bool rejected=false;try{(void)loadGltfAsset((root/"invalid-filter.gltf").string());}catch(const std::exception&){rejected=true;}
  if(!rejected)throw std::runtime_error("Invalid glTF sampler enums must fail");
 }
 auto lookup=samplerSource;
 if(!stbi_write_png((root/"lookup.png").string().c_str(),4,1,4,pixels,16))throw std::runtime_error("Cannot create lookup sampler fixture");
 lookup["images"][image]["uri"]="lookup.png";
 lookup["materials"][0]["occlusionTexture"]={{"index",texture}};
 lookup["materials"][0]["extras"]["azureRenderMaterial"]={{"schemaVersion",2},{"class","generic"},{"features",{"diffuse-ramp"}},
     {"diffuseRamp",{{"schemaVersion",1},{"texture",texture}}}};
 std::ofstream(root/"lookup-sampler.gltf")<<lookup.dump();
 const auto lookupAsset=loadGltfAsset((root/"lookup-sampler.gltf").string());
 const auto& lookupSettings=lookupAsset.materials[0].textureSettings[4];
 if(lookupSettings.mipPolicy!="none"||lookupSettings.wrapU!="clamp"||lookupSettings.wrapV!="clamp"
     ||lookupSettings.sampler(1,1).effectiveMagFilter()!=VK_FILTER_LINEAR)
  throw std::runtime_error("Dedicated lookup sampling must retain ownership when a standard slot shares its image");
 std::ofstream(root/"lookup.png.azmeta")<<nlohmann::json{{"schemaVersion",1},{"id","00000000-0000-4000-8000-000000000002"},
     {"settings",{{"texture",{{"schemaVersion",2},{"magFilter","nearest"}}}}},{"dependencies",nlohmann::json::array()}}.dump();
 bool lookupRejected=false;try{(void)loadGltfAsset((root/"lookup-sampler.gltf").string());}catch(const std::exception&){lookupRejected=true;}
 if(!lookupRejected)throw std::runtime_error("Independent magnification metadata must obey fixed lookup ownership");
 std::filesystem::remove(root/"lookup.png.azmeta");
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
 // Independent RGBA inputs must survive the production loader unchanged.
 const unsigned char sdfPixels[]={32,96,160,224,32,96,160,224,32,96,160,224,32,96,160,224};
 const unsigned char maskPixels[]={17,83,149,215,17,83,149,215,17,83,149,215,17,83,149,215};
 stbi_write_png((root/"face-sdf.png").string().c_str(),2,2,4,sdfPixels,8);
 stbi_write_png((root/"face-mask.png").string().c_str(),2,2,4,maskPixels,8);
 auto face=source;face["materials"][0]["alphaMode"]="OPAQUE";
 const auto addTexture=[&](const char* uri){const int im=static_cast<int>(face["images"].size());face["images"].push_back({{"uri",uri}});
     const int tx=static_cast<int>(face["textures"].size());face["textures"].push_back({{"source",im}});return tx;};
 const int sdf=addTexture("face-sdf.png"),mask=addTexture("face-mask.png"),ramp=addTexture("face-sdf.png");
 face["nodes"].push_back({{"name","FaceHead"}});
 face["materials"][0]["extras"]["azureRenderMaterial"]={{"schemaVersion",2},{"class","face"},{"features",{"face-sdf-eligible"}},
     {"faceSdf",{{"schemaVersion",2},{"texture",sdf},{"maskTexture",mask},{"rampTexture",ramp},{"texCoord",0},{"headNode","FaceHead"},
         {"headBasis",{1,0,0,0,1,0,0,0,1}},{"lightAdjustment",{0,0,0}}}}};
 std::ofstream(root/"face.gltf")<<face.dump();
 const auto dualFace=loadGltfAsset((root/"face.gltf").u8string());const auto& f=dualFace.materials[0];
 if(f.faceSdf.pixels!=std::vector<std::uint8_t>(sdfPixels,sdfPixels+16)
     ||f.styleMaskPixels!=std::vector<std::uint8_t>(maskPixels,maskPixels+16)
     ||f.matcapPixels!=f.faceSdf.pixels||f.textureSettings[5].colorSpace!="linear"||!(f.materialFeatures&2048U))
     throw std::runtime_error("Dual face SDF must preserve independent RGBA and linear ramp samples");
 for(const char* key:{"texture","maskTexture","rampTexture"}) {
     auto bad=face;bad["materials"][0]["extras"]["azureRenderMaterial"]["faceSdf"][key]=-1;
     std::ofstream(root/"bad-face.gltf")<<bad.dump();bool denied=false;
     try{(void)loadGltfAsset((root/"bad-face.gltf").u8string());}catch(const std::exception&){denied=true;}
     if(!denied)throw std::runtime_error("Every dual face input requires a valid image");
 }
 for(const auto basis:nlohmann::json::array({{1,0,0,1,0,0,0,0,1},{2,0,0,0,1,0,0,0,1},{1,0,0}})) {
     auto bad=face;bad["materials"][0]["extras"]["azureRenderMaterial"]["faceSdf"]["headBasis"]=basis;
     std::ofstream(root/"bad-basis.gltf")<<bad.dump();bool denied=false;
     try{(void)loadGltfAsset((root/"bad-basis.gltf").u8string());}catch(const std::exception&){denied=true;}
     if(!denied)throw std::runtime_error("Face basis must contain three orthonormal axes");
 }
 auto conflicting=face;auto other=face["materials"][0];
 other["extras"]["azureRenderMaterial"]["faceSdf"]["headBasis"]={-1,0,0,0,1,0,0,0,1};conflicting["materials"].push_back(other);
 std::ofstream(root/"conflicting-face.gltf")<<conflicting.dump();bool frameRejected=false;
 try{(void)loadGltfAsset((root/"conflicting-face.gltf").u8string());}catch(const std::exception&){frameRejected=true;}
 if(!frameRejected)throw std::runtime_error("One shared mesh instance must declare a consistent face frame");
 auto legacy=face;auto& old=legacy["materials"][0]["extras"]["azureRenderMaterial"]["faceSdf"];
 old={{"schemaVersion",1},{"texture",sdf},{"texCoord",0},{"headNode","FaceHead"},{"channel","g"},{"maskChannel","a"},
     {"shadowOnLowValues",true},{"horizontalAxis","left-to-right"}};
 std::ofstream(root/"legacy-face.gltf")<<legacy.dump();const auto legacyFace=loadGltfAsset((root/"legacy-face.gltf").u8string());
 if(legacyFace.materials[0].faceSdf.pixels[0]!=96||legacyFace.materials[0].faceSdf.pixels[3]!=224
     ||(legacyFace.materials[0].materialFeatures&2048U))throw std::runtime_error("Face SDF v1 channel contract must remain available");
 std::filesystem::remove_all(root);return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';std::filesystem::remove_all(root);return 1;}}
