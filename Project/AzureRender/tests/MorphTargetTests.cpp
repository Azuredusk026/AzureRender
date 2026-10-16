#include "assets/GltfLoader.hpp"
#include <nlohmann/json.hpp>
#include <cstring>
#include <iostream>
#include <stdexcept>
using Json=nlohmann::json;
namespace {
void require(bool b,const char* message){if(!b)throw std::runtime_error(message);}
std::string base64(const std::vector<unsigned char>& bytes){
    const char* alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;unsigned bits=0;int count=0;
    for(auto b:bytes){bits=(bits<<8)|b;count+=8;while(count>=6){count-=6;out+=alphabet[(bits>>count)&63];}}
    if(count)out+=alphabet[(bits<<(6-count))&63];while(out.size()%4)out+='=';return out;
}
Json fixture(unsigned targets,bool animation=false){
    std::vector<float> values={0,0,0,1,0,0,0,1,0, 0,0,1,0,0,1,0,0,1, 0,2};
    if(animation)for(unsigned key=0;key<2;++key)for(unsigned i=0;i<targets;++i)values.push_back(key?float(i+1):0);
    std::vector<unsigned char> bytes(values.size()*4);std::memcpy(bytes.data(),values.data(),bytes.size());
    Json j={{"asset",{{"version","2.0"}}},{"buffers",Json::array({{{"uri","data:application/octet-stream;base64,"+base64(bytes)},{"byteLength",bytes.size()}}})},
      {"bufferViews",Json::array({{{"buffer",0},{"byteLength",bytes.size()}}})},
      {"accessors",Json::array({{{"bufferView",0},{"componentType",5126},{"type","VEC3"},{"count",3}},
          {{"bufferView",0},{"byteOffset",36},{"componentType",5126},{"type","VEC3"},{"count",3}},
          {{"bufferView",0},{"byteOffset",72},{"componentType",5126},{"type","SCALAR"},{"count",2}}})},
      {"nodes",Json::array({{{"mesh",0}}})},{"scenes",Json::array({{{"nodes",Json::array({0})}}})},{"scene",0}};
    Json primitive={{"attributes",{{"POSITION",0}}},{"mode",4}};
    if(targets){primitive["targets"]=Json::array();for(unsigned i=0;i<targets;++i)primitive["targets"].push_back({{"POSITION",1},{"NORMAL",1},{"TANGENT",1}});}
    j["meshes"]=Json::array({{{"primitives",Json::array({primitive})},{"weights",std::vector<float>(targets,.25F)}}});
    if(animation){j["accessors"].push_back({{"bufferView",0},{"byteOffset",80},{"componentType",5126},{"type","SCALAR"},{"count",2*targets}});
        j["animations"]=Json::array({{{"samplers",Json::array({{{"input",2},{"output",3},{"interpolation","LINEAR"}}})},
          {"channels",Json::array({{{"sampler",0},{"target",{{"node",0},{"path","weights"}}}}})}}});}
    return j;
}
}
int main(){try{
    for(unsigned count:{0U,1U,2U,8U,64U}){auto asset=loadEmbeddedGltfAsset(fixture(count).dump());require(asset.morphTargetCount==count,"Target count must be preserved");}
    bool rejected=false;try{(void)loadEmbeddedGltfAsset(fixture(65).dump());}catch(const std::exception&){rejected=true;}require(rejected,"65 targets must exceed the declared budget");
    auto animated=loadEmbeddedGltfAsset(fixture(8,true).dump());
    require(animated.animations.size()==1,"Weights clip must import");
    const auto middle=sampleAnimationPose(animated,0,1,false);
    require(middle.morphWeights.at(0).at(7)==4,"Variable-width weights must interpolate");
    require(bindAnimationPose(animated).morphWeights.at(0).at(0)==.25F,"Mesh defaults must survive");
    auto defaults=fixture(8);defaults["nodes"][0]["weights"]=std::vector<float>(8,.75F);
    require(bindAnimationPose(loadEmbeddedGltfAsset(defaults.dump())).morphWeights[0][7]==.75F,"Node defaults must override mesh defaults");
    auto sparse=fixture(1);sparse["accessors"][1].erase("bufferView");sparse["accessors"][1].erase("byteOffset");
    sparse["buffers"].push_back({{"uri","data:application/octet-stream;base64,AQAAAAAAAEAAAAAAAABAQA=="},{"byteLength",16}});
    sparse["bufferViews"].push_back({{"buffer",1},{"byteLength",1}});
    sparse["bufferViews"].push_back({{"buffer",1},{"byteOffset",4},{"byteLength",12}});
    sparse["accessors"][1]["sparse"]={{"count",1},{"indices",{{"bufferView",1},{"componentType",5121}}},{"values",{{"bufferView",2}}}};
    const auto decoded=loadEmbeddedGltfAsset(sparse.dump());
    require(decoded.morphRanges[0].targets[0].positions[0][0]==0&&decoded.morphRanges[0].targets[0].positions[1][0]==2,"Sparse bufferless targets must expand");
    sparse["accessors"][1]["sparse"]["indices"]["byteOffset"]=99;
    rejected=false;try{(void)loadEmbeddedGltfAsset(sparse.dump());}catch(const std::exception&){rejected=true;}require(rejected,"Sparse range violations must reject");
    auto bad=fixture(8,true);bad["accessors"][3]["count"]=8;
    rejected=false;try{(void)loadEmbeddedGltfAsset(bad.dump());}catch(const std::exception&){rejected=true;}require(rejected,"Weight width mismatch must reject");
    require(animated.morphRanges[0].targets[7].normals.size()==3,"Normal deltas must survive");
    auto scaled=fixture(1);scaled["nodes"][0]["scale"]={2,3,4};
    const auto scaledAsset=loadEmbeddedGltfAsset(scaled.dump());
    require(std::abs(scaledAsset.morphRanges[0].targets[0].normals[0][2]-.75F)<1e-6F,"Static normal morph must preserve base normalization scale");
    std::cout<<"Morph target budgets and weight animation passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
