#include "assets/UnlitTextureLayers.hpp"
#include <nlohmann/json.hpp>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace azurerender;
using Json=nlohmann::json;
namespace {
unsigned checks=0;
void require(bool condition,const char* message) {++checks;if(!condition)throw std::runtime_error(message);}
void near(float actual,float expected,const char* message) {require(std::abs(actual-expected)<2e-6F,message);}
Json fixture() {
 return {{"schemaVersion",2},{"lighting","unlit"},{"layers",Json::array({{
  {"role","base"},{"texture",0},{"sampleMode","rgba"},
  {"uv",{{"set",0},{"scaleOffset",{1,1,0,0}},{"rotation",{1,0,0,1}},{"speed",{0,0}}}}}})},
  {"tint",{1.3,1.3,1.3,1}},{"blendTint",{0,0,0,0}},
  {"clock",{{"rate",1},{"offsetSeconds",0}}},
  {"softIntersection",{{"distance",1.5},{"bias",0}}},
  {"highlightExpansion",{{"threshold",1},{"gain",3}}},
  {"postExposureWeight",1},{"globalColorWeight",1}};
}
void reject(const Json& json,std::size_t textures=2) {
 bool failed=false;try{(void)parseUnlitTextureLayers(json,textures);}catch(const std::invalid_argument&){failed=true;}
 require(failed,"Invalid layer profile must fail before resource consumption");
}
void parsing() {
 const auto base=fixture();const auto p=parseUnlitTextureLayers(base,2);
 require(p.present&&p.layerCount==1&&p.layers[0].texture==0,"Base layer binding");
 auto invalid=base;
 for(const char* key:{"schemaVersion","lighting","layers","tint","clock","softIntersection","highlightExpansion","blendTint","postExposureWeight","globalColorWeight"}) {invalid=base;invalid.erase(key);reject(invalid);}
 for(const Json& v:{Json(nullptr),Json(true),Json(1.5),Json(1),Json(3)}) {invalid=base;invalid["schemaVersion"]=v;reject(invalid);}
 for(const Json& v:{Json(nullptr),Json(true),Json::array(),Json(1)})reject(v);
 invalid=base;invalid["unknown"]=nullptr;reject(invalid);
 invalid=base;invalid["lighting"]="lit";reject(invalid);
 for(const Json& value:{Json(0),Json(-1),Json(true),Json("1.5"),Json(nullptr),Json(1e100)}) {invalid=base;invalid["softIntersection"]["distance"]=value;reject(invalid);}
 invalid=base;invalid["clock"]["rate"]=std::numeric_limits<double>::infinity();reject(invalid);
 invalid=base;invalid["clock"]["extra"]=0;reject(invalid);
 invalid=base;invalid["tint"]={1,1,1};reject(invalid);
 for(const char* key:{"postExposureWeight","globalColorWeight"}) {invalid=base;invalid[key]=1.01;reject(invalid);invalid[key]=-.01;reject(invalid);}
 for(const Json& texture:{Json(-1),Json(2),Json(0.5),Json(true),Json(nullptr)}) {invalid=base;invalid["layers"][0]["texture"]=texture;reject(invalid);}
 reject(base,0);
 invalid=base;invalid["layers"]=Json::array();reject(invalid);
 invalid=base;invalid["layers"].push_back(base["layers"][0]);reject(invalid);
 invalid["layers"][1]["role"]="mask";require(parseUnlitTextureLayers(invalid,2).layerCount==2,"Mask layer allowed");
 invalid["layers"].push_back(base["layers"][0]);reject(invalid);
 invalid=base;invalid["layers"][0]["role"]="mask";reject(invalid);
 invalid=base;invalid["layers"][0]["sampleMode"]="unknown";reject(invalid);
 invalid=base;invalid["layers"][0]["uv"]["set"]=1;reject(invalid);
 invalid=base;invalid["layers"][0]["uv"]["speed"]={0};reject(invalid);
 invalid=base;invalid["layers"][0]["uv"]["rotation"]={1,0,0,std::numeric_limits<double>::quiet_NaN()};reject(invalid);
 invalid=base;invalid["layers"][0]["unknown"]=0;reject(invalid);
 for(const char* key:{"rate","offsetSeconds"}) {invalid=base;invalid["clock"][key]=true;reject(invalid);}
 for(const char* key:{"threshold","gain"}) {invalid=base;invalid["highlightExpansion"][key]=-.1;reject(invalid);}
 invalid=base;invalid["layers"][0]["uv"]["set"]=false;reject(invalid);
 invalid=base;invalid["layers"][0]["uv"]["unknown"]=1;reject(invalid);
 invalid=base;invalid["layers"][0]["role"]=nullptr;reject(invalid);
 require(parseUnlitTextureLayers(base,2).tint==p.tint,"Rejected profiles cannot mutate reusable inputs");
}
void numerical() {
 auto p=parseUnlitTextureLayers(fixture(),2);
 const auto color=evaluateUnlitTextureLayers(p,{.8F,.4F,1,.5F},{1,1,1,1},.5F,.4F,{});
 near(color[0],1.16F,"Highlight R before alpha");near(color[1],.52F,"Highlight G before alpha");near(color[2],2.2F,"Highlight B before alpha");near(color[3],.1F,"One node opacity application");
 near(color[0]*color[3],.116F,"Straight alpha compositing");
 const float scene[]={5,-5,4,7,100000,5},effect[]={4.25F,-4.25F,4.25F,4.25F,4.25F,4.25F};
 const float expected[]={.5F,.5F,0,1,1,.7F};
 for(unsigned i=0;i<6;++i)near(evaluateSoftIntersection(scene[i],effect[i],1.5F,i==5?.3F:0),expected[i],"Signed view-depth soft intersection");
 auto layer=p.layers[0];layer.scaleOffset={1.5F,.3F,0,0};layer.rotation={-4.371139e-8F,1,-1,-4.371139e-8F};layer.speed={0,.1F};
 for(unsigned i=0;i<3;++i){const auto uv=evaluateUnlitLayerUv(layer,{.25F,.75F},i);near(uv[0],.375F-.15F*i,"Native rotated scrolling U");near(uv[1],.075F,"Native rotated scrolling V");}
 p.clockRate=.5F;p.clockOffsetSeconds=.25F;const double times[]={0,1,1,2};const float tau[]={.25F,.75F,.75F,1.25F};
 for(unsigned i=0;i<4;++i)near(static_cast<float>(evaluateUnlitMaterialTime(p,times[i])),tau[i],"Host time pause and replay");
 p.highlightGain=0;p.tint={1,1,1,1};p.layerCount=2;p.layers[1].role=UnlitTextureLayer::Role::Mask;
 const auto mask=evaluateUnlitTextureLayers(p,{.8F,.4F,1,.5F},{.2F,.8F,.5F,.6F},1,1,{});
 const float maskExpected[]={.16F,.32F,.5F,.3F};for(unsigned i=0;i<4;++i)near(mask[i],maskExpected[i],"Independent RGBA mask channels");
 p.layers[1].role=UnlitTextureLayer::Role::Blend;p.blendTint={2,1,.5F,.5F};
 const auto blend=evaluateUnlitTextureLayers(p,{.2F,.3F,.4F,.7F},{.5F,.25F,.1F,.4F},1,1,{});
 const float blendExpected[]={.75F,.4375F,.4275F,.7F};for(unsigned i=0;i<4;++i)near(blend[i],blendExpected[i],"Blend weight saturation follows multiplication");
 p.layerCount=1;p.layers[0].sampleMode=UnlitTextureLayer::SampleMode::RedAsAlpha;
 const auto red=evaluateUnlitTextureLayers(p,{.25F,.8F,.9F,.1F},{},1,1,{});
 near(red[0],1,"Red-as-alpha white RGB");near(red[3],.25F,"Red-as-alpha independent coverage");
 p.layers[0].sampleMode=UnlitTextureLayer::SampleMode::Rgba;UnlitEffectFrame frame;frame.exposureGain=2;frame.saturation=0;frame.colorGain={2,1,.5F};
 const auto mapped=evaluateUnlitTextureLayers(p,{1,.5F,.25F,.8F},{},1,1,frame);const float luminance=2*(.212673F+.5F*.715152F+.25F*.072175F);
 near(mapped[0],luminance*2,"Global color follows luminance saturation");near(mapped[1],luminance,"Global color gain");near(mapped[2],luminance*.5F,"Global blue gain");near(mapped[3],.8F,"Color mapping leaves alpha independent");
 p.postExposureWeight=0;p.globalColorWeight=0;
 const auto independent=evaluateUnlitTextureLayers(p,{1,.5F,.25F,.8F},{},1,1,frame);
 near(independent[0],1,"Zero frame weight preserves source RGB");near(independent[1],.5F,"Zero frame color weight");
 p.tint[3]=2;
 near(evaluateUnlitTextureLayers(p,{.2F,.3F,.4F,.75F},{},.75F,.4F,{})[3],.4F,
  "Coverage saturates after soft intersection and before node opacity");
 near(evaluateUnlitTextureLayers(p,{.2F,.3F,.4F,.75F},{},.5F,.4F,{})[3],.3F,
  "Unsaturated soft coverage remains proportional");
 p.tint[3]=-1;
 near(evaluateUnlitTextureLayers(p,{.2F,.3F,.4F,.75F},{},1,.4F,{})[3],0,
  "Negative material alpha contributes zero coverage");
 p.tint[3]=1;
 const auto bad=[&](auto callback){bool failed=false;try{callback();}catch(const std::invalid_argument&){failed=true;}require(failed,"Invalid runtime input must fail explicitly");};
 bad([&]{(void)evaluateUnlitMaterialTime(p,std::numeric_limits<double>::infinity());});
 bad([&]{(void)evaluateUnlitLayerUv(layer,{.2F,.3F},std::numeric_limits<double>::quiet_NaN());});
 bad([&]{(void)evaluateSoftIntersection(1,1,0,0);});
 bad([&]{(void)evaluateUnlitTextureLayers(p,{1,1,1,1},{},1,1.1F,frame);});
 frame.exposureGain=-1;bad([&]{(void)evaluateUnlitTextureLayers(p,{1,1,1,1},{},1,1,frame);});
}
void capturedExamples(const char* path) {
 std::ifstream stream(path);if(!stream)throw std::runtime_error("Cannot open supplied source contract");const Json contract=Json::parse(stream);
 unsigned profiles=0;
 for(const auto& example:contract.at("publicProfileExamples")) {
  const auto& surface=example.at("surface");std::size_t count=0;
  for(const auto& layer:surface.at("layers"))count=std::max(count,layer.at("texture").get<std::size_t>()+1);
  const auto profile=parseUnlitTextureLayers(surface,count);require(profile.present,"Actual source profile parses");++profiles;
  if(profile.layerCount==2&&profile.layers[1].role==UnlitTextureLayer::Role::Blend) {
   for(const auto& oracle:contract.at("oracleFixtures").at("uv")) {
    const auto uv=evaluateUnlitLayerUv(profile.layers[1],oracle.at("uv").get<std::array<float,2>>(),oracle.at("tau").get<double>());
    for(unsigned i=0;i<2;++i)near(uv[i],oracle.at("expected")[i].get<float>(),"Actual source rolling oracle");
   }
  }
 }
 require(profiles==2,"Both recorded source examples verified");
}
}
int main(int argc,char** argv){try{if(argc>2)throw std::invalid_argument("Expected optional source contract path");parsing();numerical();if(argc==2)capturedExamples(argv[1]);std::cout<<"Passed "<<checks<<" unlit layer checks\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
