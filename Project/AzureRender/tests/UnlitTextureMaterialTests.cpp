#include "assets/GltfLoader.hpp"
#include "assets/MaterialSurfaceProfile.hpp"
#include "assets/MaterialData.hpp"
#include "runtime/LevelRenderSettings.hpp"
#include "diagnostics/RuntimeDiagnostics.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <chrono>
#include <stb_image_write.h>

using namespace azurerender;
using Json=nlohmann::json;
namespace {
unsigned checks=0;
void require(bool condition,const char* message){++checks;if(!condition)throw std::runtime_error(message);}
Json surface() {
 auto layer=[](const char* role,int texture){return Json{{"role",role},{"texture",texture},{"sampleMode","rgba"},
  {"uv",{{"set",0},{"scaleOffset",{1,1,0,0}},{"rotation",{1,0,0,1}},{"speed",{0,.1}}}}};};
 return {{"schemaVersion",2},{"lighting","unlit"},{"layers",Json::array({layer("base",0),layer("mask",1)})},
  {"tint",{1.3,1.3,1.3,1}},{"blendTint",{0,0,0,0}},{"clock",{{"rate",1},{"offsetSeconds",0}}},
  {"softIntersection",{{"distance",1.5},{"bias",0}}},{"highlightExpansion",{{"threshold",1},{"gain",3}}},
  {"postExposureWeight",1},{"globalColorWeight",1}};
}
}
int main(int argc,char** argv){try{
 require(argc==2,"Fixture path required");const auto original=std::filesystem::absolute(argv[1]);Json source;std::ifstream(original)>>source;
 for(const char* section:{"buffers","images"})for(auto& entry:source[section])if(entry.contains("uri")) {
  const auto uri=entry["uri"].get<std::string>();if(uri.rfind("data:",0)!=0)entry["uri"]=(original.parent_path()/uri).generic_string();
 }
 const auto folder=std::filesystem::temp_directory_path()/("azure-unlit-material-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));std::filesystem::create_directory(folder);
 struct Cleanup{std::filesystem::path p;~Cleanup(){std::error_code e;std::filesystem::remove_all(p,e);}}cleanup{folder};const auto path=folder/"fixture.gltf";
 unsigned bufferIndex=0;
 for(auto& entry:source["buffers"])if(entry.contains("uri")) {
  const auto uri=entry["uri"].get<std::string>();if(uri.rfind("data:",0)==0)continue;
  const auto name="geometry-"+std::to_string(bufferIndex++)+".bin";std::filesystem::copy_file(uri,folder/name);entry["uri"]=name;
 }
 const unsigned char basePixels[]={255,64,128,192, 32,192,64,128, 64,128,255,64, 192,255,32,255};
 const unsigned char maskPixels[]={64,192,128,160, 128,64,192,96, 192,128,64,224, 255,32,128,192};
 require(stbi_write_png((folder/"base.png").string().c_str(),2,2,4,basePixels,8)!=0,"Base PNG fixture");
 require(stbi_write_png((folder/"mask.png").string().c_str(),2,2,4,maskPixels,8)!=0,"Mask PNG fixture");
 source["images"]=Json::array({{{"uri","base.png"}},{{"uri","mask.png"}}});
 source["textures"]=Json::array({{{"source",0}},{{"source",1}}});
 source["materials"]=Json::array({{{"name","PublicSignalLayers"},{"alphaMode","BLEND"},{"doubleSided",true},
  {"extras",{{"azureRenderMaterial",{{"schemaVersion",2},{"class","generic"},{"features",{"unlit-surface","unlit-texture-layers"}},{"surface",surface()}}}}}}});
 for(auto& mesh:source["meshes"])for(auto& p:mesh["primitives"])p["material"]=0;
 source["samplers"]=Json::array({{{"wrapS",10497},{"wrapT",10497},{"minFilter",9729},{"magFilter",9729}},
  {{"wrapS",33071},{"wrapT",10497},{"minFilter",9985},{"magFilter",9729}}});
 source["textures"][0]["sampler"]=0;source["textures"][1]["sampler"]=1;
 const auto load=[&](const Json& doc){std::ofstream(path)<<doc.dump();return loadGltfAsset(path.string());};
 const auto reject=[&](const Json& doc){bool failed=false;try{(void)load(doc);}catch(const std::exception& e){failed=std::string(e.what()).find("unlit")!=std::string::npos;}require(failed,"Invalid graph must identify unlit ownership");};
 const auto asset=load(source);const auto& m=asset.materials.at(0);
 require(!asset.vertices.empty()&&(m.materialFeatures&(1U<<24U)),"Layer capability reaches production material");
 require(m.textureSettings[0].wrapU=="repeat"&&m.textureSettings[4].wrapU=="clamp"&&m.textureSettings[4].wrapV=="repeat","Independent per-axis samplers reach material slots");
 require(m.textureSettings[0].mipPolicy=="none"&&m.textureSettings[4].mipFilter==TextureImportSettings::MipFilter::Nearest,"Layer mip sampling preserved");
 require(!m.styleMaskPixels.empty()&&m.styleMaskWidth>0,"Second layer image reaches owned slot");
 const auto packed=packMaterialSurfaceProfile(m);
 auto distortionSource=source;auto& distortionSurface=distortionSource["materials"][0]["extras"]["azureRenderMaterial"]["surface"];
 distortionSurface["baseUvDistortion"]={{"texture",1},{"channels","red"},
  {"uv",distortionSurface["layers"][0]["uv"]},{"scale",{.1,-.1}},{"bias",{.2,-.2}}};
 const auto distortionMaterial=load(distortionSource).materials.at(0);const auto distortionPacked=packMaterialSurfaceProfile(distortionMaterial);
 require(distortionPacked.rows[16][2]==1&&distortionPacked.rows[20]==std::array<float,4>{.1F,-.1F,.2F,-.2F},
  "Base UV distortion enable and remap reach material table");
 require(distortionMaterial.specularEmissivePixels==distortionMaterial.styleMaskPixels
  &&distortionMaterial.textureSettings[3].wrapU=="clamp"
  &&distortionMaterial.textureSettings[3].mipFilter==TextureImportSettings::MipFilter::Nearest,
  "Base UV distortion owns slot three and its sampler");
 require(packed.rows[0]==std::array<float,4>{1,1,0,0}&&packed.rows[2]==std::array<float,4>{0,.1F,0,.1F},"UV and time rows reach public material table");
 require(packed.rows[5]==std::array<float,4>{1,0,1.5F,0}&&packed.rows[6]==std::array<float,4>{1.3F,1.3F,1.3F,1},"Clock and straight tint packing");
 require(packed.rows[9]==std::array<float,4>{0,0,1,1},"Layer modes and soft intersection packing");
 auto coverageSource=source;auto& coverageSurface=coverageSource["materials"][0]["extras"]["azureRenderMaterial"]["surface"];
 coverageSurface["coverageLayer"]={{"texture",1},{"sampleMode","red-as-alpha"},
  {"uv",coverageSurface["layers"][0]["uv"]},{"threshold",-.25},{"gain",2}};
 const auto coverageMaterial=load(coverageSource).materials.at(0);const auto coveragePacked=packMaterialSurfaceProfile(coverageMaterial);
 require(coveragePacked.rows[11][3]==1&&coveragePacked.rows[16]==std::array<float,4>{-.25F,2,0,0},
  "Coverage enable and remap reach the material table");
 require(coverageMaterial.shadowColorLutPixels==coverageMaterial.styleMaskPixels
  &&coverageMaterial.shadowColorLutWidth==2&&coverageMaterial.shadowColorLutHeight==2,
  "Coverage owns slot eight without lookup-table dimension rules");
 require(coverageMaterial.textureSettings[8].wrapU=="clamp"
  &&coverageMaterial.textureSettings[8].mipFilter==TextureImportSettings::MipFilter::Nearest,
  "Coverage texture preserves its own sampler");
 auto fadedSource=source;fadedSource["materials"][0]["extras"]["azureRenderMaterial"]["surface"]["viewDepthFade"]=
  {{"near",{.5,1}},{"far",{100,150}}};
 const auto fadedPacked=packMaterialSurfaceProfile(load(fadedSource).materials.at(0));
 require(fadedPacked.rows[10]==std::array<float,4>{.5F,1,100,150}
  &&fadedPacked.rows[11]==std::array<float,4>{1,0,0,0},"View-depth fade uses reserved material rows");
 auto coloredSource=source;coloredSource["materials"][0]["extras"]["azureRenderMaterial"]["surface"]["vertexColor"]=true;
 require(packMaterialSurfaceProfile(load(coloredSource).materials.at(0)).rows[11][1]==1,
  "Explicit vertex color consumption reaches the material table");
 auto document=MaterialData::defaults("azure.profile");
 auto viewSource=source;auto& viewSurface=viewSource["materials"][0]["extras"]["azureRenderMaterial"]["surface"];
 viewSurface["viewCoverage"]={{"bias",1.23},{"exponent",1},{"invertWeight",0},{"weight",.988}};
 viewSurface["softIntersection"]["enabled"]=false;
 viewSurface["composite"]="additive";
 const auto viewPacked=packMaterialSurfaceProfile(load(viewSource).materials.at(0));
 require(viewPacked.rows[9][3]==0&&viewPacked.rows[11][2]==1
  &&viewPacked.rows[12]==std::array<float,4>{1.23F,1,0,.988F},"View coverage and explicit scene-depth bypass reach the material table");
 document.surface={{"asset",{{"version","2.0"}}},{"materials",source["materials"]},{"textures",source["textures"]},{"samplers",source["samplers"]}};
 const std::string baseId="11111111-1111-4111-8111-111111111111",maskId="22222222-2222-4222-8222-222222222222";
 document.textures={{"0",{{"asset",baseId},{"usage","albedo"}}},{"1",{{"asset",maskId},{"usage","albedo"}}}};
 const auto restored=MaterialData::parse(document.encode());
 auto distortionDocument=document;distortionDocument.surface["materials"]=distortionSource["materials"];
 const auto distortionStandalone=MaterialData::parse(distortionDocument.encode()).interpret(
  [&](const std::string& id){return folder/(id==baseId?"base.png":"mask.png");});
 require(packMaterialSurfaceProfile(distortionStandalone).rows==distortionPacked.rows
  &&distortionStandalone.specularEmissivePixels==distortionMaterial.specularEmissivePixels,
  "Complete document preserves base UV distortion pixels and parameters");
 auto coverageDocument=document;coverageDocument.surface["materials"]=coverageSource["materials"];
 const auto coverageStandalone=MaterialData::parse(coverageDocument.encode()).interpret(
  [&](const std::string& id){return folder/(id==baseId?"base.png":"mask.png");});
 require(packMaterialSurfaceProfile(coverageStandalone).rows==coveragePacked.rows
  &&coverageStandalone.shadowColorLutPixels==coverageMaterial.shadowColorLutPixels,
  "Complete document preserves independent coverage pixels and parameters");
 auto viewDocument=document;viewDocument.surface["materials"]=viewSource["materials"];
 const auto viewStandalone=MaterialData::parse(viewDocument.encode()).interpret(
  [&](const std::string& id){return folder/(id==baseId?"base.png":"mask.png");});
 require(packMaterialSurfaceProfile(viewStandalone).rows==viewPacked.rows,"Complete document preserves view coverage and scene-depth selection");
 require(viewStandalone.unlitTextureLayers.composite==UnlitTextureLayers::Composite::Additive,
  "Complete document preserves additive composition independently of GPU parameter rows");
 auto fadedDocument=document;fadedDocument.surface["materials"]=fadedSource["materials"];
 const auto fadedStandalone=MaterialData::parse(fadedDocument.encode()).interpret(
  [&](const std::string& id){return folder/(id==baseId?"base.png":"mask.png");});
 require(packMaterialSurfaceProfile(fadedStandalone).rows==fadedPacked.rows,
  "Complete material documents preserve view-depth fade");
 auto coloredDocument=document;coloredDocument.surface["materials"]=coloredSource["materials"];
 require(MaterialData::parse(coloredDocument.encode()).interpret(
  [&](const std::string& id){return folder/(id==baseId?"base.png":"mask.png");}).unlitTextureLayers.useVertexColor,
  "Complete material documents preserve explicit vertex color consumption");
 const auto interpreted=restored.interpret([&](const std::string& id){return folder/(id==baseId?"base.png":"mask.png");});
 require(packMaterialSurfaceProfile(interpreted).rows==packed.rows,"Complete standalone material document carries layer contract");
 require(interpreted.textureSettings[4].wrapU=="clamp"&&interpreted.styleMaskPixels==m.styleMaskPixels,"Standalone layer sampler and pixels agree with model import");
 const auto metadata=[&](const char* name,const std::string& id,const Json& settings){
  std::ofstream(folder/(std::string(name)+".azmeta"))<<Json{{"schemaVersion",1},{"id",id},
   {"settings",{{"texture",settings}}},{"dependencies",Json::array()}}.dump();
 };
 metadata("base.png",baseId,{{"mipBias",-1.25}});metadata("mask.png",maskId,{{"mipBias",.75}});
 const auto biased=load(source).materials.at(0);
 require(biased.textureSettings[0].mipBias==-1.25F&&biased.textureSettings[4].mipBias==.75F,
  "Layer image metadata carries independent mip bias");
 require(biased.textureSettings[0].wrapU=="repeat"&&biased.textureSettings[4].wrapU=="clamp"
  &&biased.textureSettings[4].mipFilter==TextureImportSettings::MipFilter::Nearest,
  "Mip bias preserves glTF sampler ownership");
 const auto biasedDocument=restored.interpret([&](const std::string& id){return folder/(id==baseId?"base.png":"mask.png");});
 require(biasedDocument.textureSettings[0].mipBias==-1.25F&&biasedDocument.textureSettings[4].mipBias==.75F,
  "Complete material documents consume layer image mip bias");
 require(packMaterialSurfaceProfile(biased).rows==packed.rows,"Sampler bias leaves surface parameters intact");
 metadata("base.png",baseId,{{"mipBias",-1},{"wrapU","clamp"}});reject(source);
 std::filesystem::remove(folder/"base.png.azmeta");std::filesystem::remove(folder/"mask.png.azmeta");
 const auto patch=MaterialData::parse({{"schemaVersion",2},{"typeId","azure.pbr"},{"mode","parameters"},
  {"parameters",{{"emissiveStrength",1}}},{"textures",Json::object()}});
 bool patchRejected=false;
 try{(void)patch.interpret([&](const std::string&){return folder/"base.png";},&interpreted);}
 catch(const std::invalid_argument& e){patchRejected=std::string(e.what()).find("complete profile")!=std::string::npos;}
 require(patchRejected,"Typed layer parameters require a complete profile document");
 auto bound=asset;const auto slot=materialSlotIds(bound).at(0);const auto boundName=bound.materials[0].name;
 const auto materialPath=folder/"layers.azmat";std::ofstream(materialPath)<<document.encode().dump();
 MaterialBinding binding;binding.path=materialPath.string();binding.textures={{baseId,(folder/"base.png").string()},{maskId,(folder/"mask.png").string()}};
 applyMaterialBindings(bound,{{slot,binding}});
 require(packMaterialSurfaceProfile(bound.materials[0]).rows==packed.rows&&bound.materials[0].name==boundName,"Public slot binding preserves complete layers and source identity");
 auto reduced=document;auto& reducedProfile=reduced.surface["materials"][0]["extras"]["azureRenderMaterial"];
 reducedProfile["features"]={"unlit-surface"};reducedProfile["surface"]={{"schemaVersion",1},{"lighting","unlit"}};
 std::ofstream(materialPath)<<reduced.encode().dump();const auto messages=RuntimeDiagnostics::instance().messages().size();
 applyMaterialBindings(bound,{{slot,binding}});
 require(RuntimeDiagnostics::instance().messages().size()==messages+1
  &&RuntimeDiagnostics::instance().messages().back().find("unlit-texture-layers")!=std::string::npos,"Full replacement reports lost layer semantics");
 RenderSettings settings;
 const Json frame={{"schemaVersion",1},{"exposureGain",2},{"saturation",.5},{"colorGain",{.5,1,2}}};
 decodeLevelRenderSettings(settings,{{"schemaVersion",20},{"unlitEffects",frame}});
 const auto encoded=encodeLevelRenderSettings(settings);
 require(encoded.at("unlitEffects")==frame,"Frame parameters round-trip in level documents");
 for(const Json& bad:Json::array({nullptr,true,{{"schemaVersion",1},{"exposureGain",true},{"saturation",1},{"colorGain",{1,1,1}}},
  {{"schemaVersion",1},{"exposureGain",-1},{"saturation",1},{"colorGain",{1,1,1}}},
  {{"schemaVersion",1},{"exposureGain",1},{"saturation",1},{"colorGain",{1,1}}}})) {
  bool failed=false;try{decodeLevelRenderSettings(settings,{{"schemaVersion",20},{"unlitEffects",bad}});}catch(const std::exception&){failed=true;}
  require(failed&&encodeLevelRenderSettings(settings)==encoded,"Invalid frame decoding is transactional");
 }
 bool oldVersionRejected=false;try{decodeLevelRenderSettings(settings,{{"schemaVersion",19},{"unlitEffects",frame}});}catch(const std::exception&){oldVersionRejected=true;}
 require(oldVersionRejected&&encodeLevelRenderSettings(settings)==encoded,"Frame settings require version 20 without mutating the target");
 auto invalid=source;
 for(const char* mode:{"OPAQUE","MASK"}){invalid=source;invalid["materials"][0]["alphaMode"]=mode;reject(invalid);}
 invalid=source;invalid["materials"][0]["extras"]["azureRenderMaterial"]["features"]={"unlit-surface"};reject(invalid);
 invalid=source;invalid["materials"][0]["extras"]["azureRenderMaterial"]["features"]={"unlit-texture-layers"};reject(invalid);
 for(const char* feature:{"emissive-mask","surface-ao","scene-refraction","hair-anisotropy"}) {invalid=source;invalid["materials"][0]["extras"]["azureRenderMaterial"]["features"].push_back(feature);reject(invalid);}
 invalid=source;invalid["textures"][1]["source"]=-1;reject(invalid);
 invalid=source;invalid["textures"][1]["sampler"]=999;reject(invalid);
 invalid=source;invalid["samplers"][1]["magFilter"]=9728;reject(invalid);
 invalid=source;invalid["textures"][0]["extensions"]["KHR_texture_transform"]={{"offset",{0,0}}};reject(invalid);
 invalid=source;invalid["materials"][0]["pbrMetallicRoughness"]["baseColorFactor"]={.5,1,1,1};reject(invalid);
 invalid=source;invalid["materials"][0]["normalTexture"]={{"index",0}};reject(invalid);
 require(packMaterialSurfaceProfile(load(source).materials.at(0)).rows==packed.rows,"Failed loads leave reusable asset interpretation intact");
 std::cout<<"Passed "<<checks<<" unlit material checks\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
