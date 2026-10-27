#include "assets/GltfLoader.hpp"
#include <stb_image_write.h>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <cmath>
#include <iostream>

using Json=nlohmann::json;
int main(){
 const auto root=std::filesystem::temp_directory_path()/("azure-material-uv-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 try{
  std::filesystem::create_directories(root);
  const float data[]={0,0,0,1,0,0,0,1,0, 0,0,1,0,0,1,0,0,1,
      0,0,1,0,0,1, 0,0,0,1,-1,0};
  const unsigned short indices[]={0,1,2};
  {std::ofstream out(root/"mesh.bin",std::ios::binary);out.write(reinterpret_cast<const char*>(data),sizeof(data));out.write(reinterpret_cast<const char*>(indices),sizeof(indices));}
  const unsigned char white[]={128,128,255,255};
  if(!stbi_write_png((root/"normal.png").string().c_str(),1,1,4,white,4))throw std::runtime_error("Cannot write UV fixture texture");
  Json source={{"asset",{{"version","2.0"}}},{"scene",0},{"scenes",Json::array({{{"nodes",{0}}}})},
    {"nodes",Json::array({{{"mesh",0}}})},{"buffers",Json::array({{{"uri","mesh.bin"},{"byteLength",sizeof(data)+sizeof(indices)}}})},
    {"bufferViews",Json::array({{{"buffer",0},{"byteOffset",0},{"byteLength",36}},{{"buffer",0},{"byteOffset",36},{"byteLength",36}},
       {{"buffer",0},{"byteOffset",72},{"byteLength",24}},{{"buffer",0},{"byteOffset",96},{"byteLength",24}},{{"buffer",0},{"byteOffset",120},{"byteLength",6}}})},
    {"accessors",Json::array({{{"bufferView",0},{"componentType",5126},{"count",3},{"type","VEC3"},{"min",{0,0,0}},{"max",{1,1,0}}},
       {{"bufferView",1},{"componentType",5126},{"count",3},{"type","VEC3"}},{{"bufferView",2},{"componentType",5126},{"count",3},{"type","VEC2"}},
       {{"bufferView",3},{"componentType",5126},{"count",3},{"type","VEC2"}},{{"bufferView",4},{"componentType",5123},{"count",3},{"type","SCALAR"}}})},
    {"images",Json::array({{{"uri","normal.png"}}})},{"textures",Json::array({{{"source",0}}})},
    {"materials",Json::array({{{"name","UV fixture"},{"normalTexture",{{"index",0},{"texCoord",0}}}}})},
    {"meshes",Json::array({{{"primitives",Json::array({{{"attributes",{{"POSITION",0},{"NORMAL",1},{"TEXCOORD_0",2},{"TEXCOORD_1",3}}},{"indices",4},{"material",0},{"mode",4}}})}}})}};
  const auto load=[&](const Json& input){std::ofstream(root/"model.gltf")<<input.dump();return loadGltfAsset((root/"model.gltf").string());};
  const auto near=[](float a,float b){return std::abs(a-b)<1e-6F;};
  const auto ordinary=load(source);
  if(!near(ordinary.vertices[1].texcoord[0],1)||!near(ordinary.vertices[0].tangent[0],1))throw std::runtime_error("UV0 default surface changed");
  auto secondary=source;secondary["materials"][0]["normalTexture"]["texCoord"]=1;
  const auto selected=load(secondary);
  if(!near(selected.vertices[1].texcoord[0],0)||!near(selected.vertices[1].texcoord[1],1))throw std::runtime_error("Selected UV1 must become the runtime surface coordinates");
  if(!near(selected.vertices[0].tangent[0],0)||!near(selected.vertices[0].tangent[1],-1))throw std::runtime_error("Generated tangent must follow the selected normal UV");
  if(selected.texcoord1.size()!=3||!selected.primitives[0].hasTexcoord1)throw std::runtime_error("Secondary source stream must remain available");
  const auto reject=[&](Json invalid){try{(void)load(invalid);}catch(const std::exception&){return;}throw std::runtime_error("Invalid surface UV input was accepted");};
  auto mixed=secondary;mixed["materials"][0]["pbrMetallicRoughness"]["baseColorTexture"]={{"index",0},{"texCoord",0}};reject(mixed);
  auto missing=secondary;missing["meshes"][0]["primitives"][0]["attributes"].erase("TEXCOORD_1");reject(missing);
  auto transformed=secondary;transformed["materials"][0]["normalTexture"]["extensions"]["KHR_texture_transform"]={{"offset",{.1,0}}};reject(transformed);
  for(int invalid:{-1,2}){auto bad=secondary;bad["materials"][0]["normalTexture"]["texCoord"]=invalid;reject(bad);}
  auto onlySecondary=secondary;onlySecondary["meshes"][0]["primitives"][0]["attributes"].erase("TEXCOORD_0");
  if(!near(load(onlySecondary).vertices[1].texcoord[1],1))throw std::runtime_error("A complete selected UV1 stream must supply the surface coordinates");
  auto allRoles=secondary;
  allRoles["materials"][0]["pbrMetallicRoughness"]={{"baseColorTexture",{{"index",0},{"texCoord",1}}},{"metallicRoughnessTexture",{{"index",0},{"texCoord",1}}}};
  allRoles["materials"][0]["emissiveTexture"]={{"index",0},{"texCoord",1}};
  allRoles["materials"][0]["occlusionTexture"]={{"index",0},{"texCoord",1}};
  if(!near(load(allRoles).vertices[1].texcoord[1],1))throw std::runtime_error("All standard surface texture roles must accept the same UV selection");
  auto multiple=source;multiple["materials"].push_back(secondary["materials"][0]);auto primitive=source["meshes"][0]["primitives"][0];primitive["material"]=1;multiple["meshes"][0]["primitives"].push_back(primitive);
  const auto independent=load(multiple);
  if(independent.primitives.size()!=2||!near(independent.vertices[1].texcoord[0],1)||!near(independent.vertices[4].texcoord[0],0))throw std::runtime_error("Primitive material UV selections must remain independent");
  const float authored[]={1,0,0,-1,1,0,0,-1,1,0,0,-1};
  {std::ofstream out(root/"mesh.bin",std::ios::binary|std::ios::app);const char padding[2]={0,0};out.write(padding,2);out.write(reinterpret_cast<const char*>(authored),sizeof(authored));}
  auto explicitTangent=secondary;explicitTangent["buffers"][0]["byteLength"]=176;
  explicitTangent["bufferViews"].push_back({{"buffer",0},{"byteOffset",128},{"byteLength",48}});
  explicitTangent["accessors"].push_back({{"bufferView",5},{"componentType",5126},{"count",3},{"type","VEC4"}});
  explicitTangent["meshes"][0]["primitives"][0]["attributes"]["TANGENT"]=5;
  const auto retained=load(explicitTangent);
  if(!near(retained.vertices[0].tangent[0],1)||!near(retained.vertices[0].tangent[3],-1))throw std::runtime_error("Authored tangent directions and handedness must remain intact");
  const unsigned char colors[]={255,0,64,255,128,32,0,128,0,255,255,64};
  {std::ofstream out(root/"mesh.bin",std::ios::binary|std::ios::app);out.write(reinterpret_cast<const char*>(colors),sizeof(colors));}
  auto colored=source;colored["buffers"][0]["byteLength"]=188;
  colored["bufferViews"].push_back({{"buffer",0},{"byteOffset",176},{"byteLength",12}});
  colored["accessors"].push_back({{"bufferView",5},{"componentType",5121},{"count",3},{"type","VEC4"},{"normalized",true}});
  colored["meshes"][0]["primitives"][0]["attributes"]["COLOR_0"]=5;
  const auto rgba=load(colored);
  if(rgba.vertexColors.size()!=3||!near(rgba.vertexColors[0][2],64.F/255)||!near(rgba.vertexColors[1][3],128.F/255))
      throw std::runtime_error("Normalized COLOR_0 must remain linear RGBA");
  if(!ordinary.vertexColors.empty())throw std::runtime_error("Absent colors must retain the optional-stream default");
  auto mixedColors=colored;mixedColors["meshes"][0]["primitives"].push_back(source["meshes"][0]["primitives"][0]);
  const auto padded=load(mixedColors);
  if(padded.vertexColors.size()!=6||padded.vertexColors[3]!=std::array<float,4>{1,1,1,1})
      throw std::runtime_error("Uncolored primitives must use white without changing color indices");
  auto invalidColor=colored;invalidColor["accessors"][5]["normalized"]=false;reject(invalidColor);
  invalidColor=colored;invalidColor["accessors"][5]["count"]=2;reject(invalidColor);
  invalidColor=colored;invalidColor["accessors"][5]["type"]="VEC2";reject(invalidColor);
  invalidColor=colored;invalidColor["bufferViews"][5]["byteLength"]=11;reject(invalidColor);
  const auto colorFixture=[&](const void* bytes,std::size_t size,int format,const char* type,bool normalized,
      unsigned stride=0,unsigned accessorOffset=0){
    const auto length=std::filesystem::file_size(root/"mesh.bin");const auto offset=(length+3)&~std::uintmax_t(3);
    {std::ofstream out(root/"mesh.bin",std::ios::binary|std::ios::app);for(auto i=length;i<offset;++i)out.put(0);out.write(static_cast<const char*>(bytes),size);}
    auto input=source;input["buffers"][0]["byteLength"]=offset+size;
    input["bufferViews"].push_back({{"buffer",0},{"byteOffset",offset},{"byteLength",size}});
    if(stride)input["bufferViews"][5]["byteStride"]=stride;
    input["accessors"].push_back({{"bufferView",5},{"componentType",format},{"count",3},{"type",type},{"normalized",normalized},{"byteOffset",accessorOffset}});
    input["meshes"][0]["primitives"][0]["attributes"]["COLOR_0"]=5;return input;
  };
  const unsigned short rgb16[]={65535,0,32768,0,65535,0,123,456,789};
  const auto rgb=load(colorFixture(rgb16,sizeof(rgb16),5123,"VEC3",true));
  if(!near(rgb.vertexColors[0][2],32768.F/65535)||rgb.vertexColors[2][3]!=1)
      throw std::runtime_error("Normalized RGB16 must preserve linear channels and implicit alpha");
  const float rgba32[]={.1F,.2F,.3F,.4F,.5F,.6F,.7F,.8F,.9F,1,0,.2F};
  const auto floatSource=colorFixture(rgba32,sizeof(rgba32),5126,"VEC4",false);
  if(load(floatSource).vertexColors[1]!=std::array<float,4>{.5F,.6F,.7F,.8F})throw std::runtime_error("Float colors must preserve their exact values");
  invalidColor=floatSource;invalidColor["accessors"][5]["normalized"]=true;reject(invalidColor);
  const unsigned char interleaved[]={0,0,255,0,64,255,0,0, 0,0,128,32,0,128,0,0, 0,0,0,255,255,64,0,0};
  if(load(colorFixture(interleaved,sizeof(interleaved),5121,"VEC4",true,8,2)).vertexColors!=rgba.vertexColors)
      throw std::runtime_error("Interleaved color offsets and strides must preserve source channels");
  float invalidFloats[12]={};invalidFloats[0]=1.01F;
  reject(colorFixture(invalidFloats,sizeof(invalidFloats),5126,"VEC4",false));
  std::filesystem::remove_all(root);std::cout<<"Material UV import checks passed\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';std::filesystem::remove_all(root);return 1;}
}
