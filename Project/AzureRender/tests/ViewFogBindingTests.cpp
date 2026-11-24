#include "render/ViewFogBindingJson.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <stdexcept>
#include <limits>
#include <cmath>
using namespace azurerender;
namespace {
unsigned checks=0;
void check(bool value,const char* text){++checks;if(!value)throw std::runtime_error(text);}
template<class F> void reject(F function){bool failed=false;try{function();}catch(const std::exception&){failed=true;}check(failed,"Invalid view fog binding must fail");}
}
int main()try{
 ViewFogBinding binding;binding.enabled=true;binding.asset="fog:/capture.azfog";binding.projection.extent={2560,1440};binding.captureView=CameraViewDescriptor{};
 const nlohmann::json encoded=binding;check(encoded.contains("schemaVersion")&&encoded["schemaVersion"]==1,"Versioned fog scene binding");
 const auto restored=encoded.get<ViewFogBinding>();check(restored.enabled&&restored.asset==binding.asset,"Resource and activation survive full JSON roundtrip");
 check(restored.projection.extent==binding.projection.extent&&restored.captureView.has_value(),"Projection and capture camera survive full JSON roundtrip");
 check(restored.match(*binding.captureView,16.F/9)==FogViewMatch::Matched,"Captured camera and aspect are compatible");
 auto moved=*binding.captureView;moved.position[0]+=.1F;check(restored.match(moved,16.F/9)==FogViewMatch::CameraMismatch,"Translated camera is rejected");
 auto changed=*binding.captureView;changed.fov+=.01F;check(restored.match(changed,16.F/9)==FogViewMatch::CameraMismatch,"Changed lens is rejected");
 check(restored.match(*binding.captureView,1)==FogViewMatch::AspectMismatch,"Changed aspect is rejected");
 auto clips=*binding.captureView;clips.nearPlane=.2F;clips.farPlane=1000;
 check(restored.match(clips,16.F/9)==FogViewMatch::Matched,"Linear view depth is independent of clip planes");
 auto turned=*binding.captureView;turned.up={1,0,0};
 check(restored.match(turned,16.F/9)==FogViewMatch::CameraMismatch,"Camera roll invalidates captured fog");
 auto scaled=*binding.captureView;for(unsigned i=0;i<3;++i)scaled.target[i]=scaled.position[i]+(scaled.target[i]-scaled.position[i])*2;
 for(float& value:scaled.up)value*=2;check(restored.match(scaled,16.F/9)==FogViewMatch::Matched,"Equivalent normalized view basis is compatible");
 auto disabled=binding;disabled.enabled=false;check(disabled.match(moved,1)==FogViewMatch::Disabled,"Disabled field does not constrain the view");
 check(fogViewMatchName(FogViewMatch::CameraMismatch)=="camera-mismatch","Observable mismatch state");
 auto bad=binding;bad.asset.clear();reject([&]{bad.validate();});bad=binding;bad.captureView.reset();reject([&]{bad.validate();});
 bad=binding;bad.captureView->target=bad.captureView->position;reject([&]{bad.validate();});bad=binding;bad.captureView->fov=std::numeric_limits<float>::quiet_NaN();reject([&]{bad.validate();});
 for(const char* field:{"schemaVersion","asset","enabled","projection","captureView"}){auto malformed=encoded;malformed.erase(field);reject([&]{(void)malformed.get<ViewFogBinding>();});}
 auto malformed=encoded;malformed["extra"]=1;reject([&]{(void)malformed.get<ViewFogBinding>();});malformed=encoded;malformed["enabled"]=1;reject([&]{(void)malformed.get<ViewFogBinding>();});
 malformed=encoded;malformed["projection"]["extent"]={true,1440};reject([&]{(void)malformed.get<ViewFogBinding>();});
 malformed=encoded;malformed["captureView"]["fieldOfViewRadians"]=true;reject([&]{(void)malformed.get<ViewFogBinding>();});
 for(const char* owner:{"schemaVersion","projection"}){
  malformed=encoded;if(std::string(owner)=="projection")malformed[owner]["schemaVersion"]=2;else malformed[owner]=2;
  reject([&]{(void)malformed.get<ViewFogBinding>();});
 }
 auto destination=binding;malformed=encoded;malformed["asset"]="changed";malformed["projection"]["extent"]={0,1440};
 reject([&]{from_json(malformed,destination);});check(destination.asset==binding.asset,"Rejected decode leaves target binding unchanged");
 ViewFogBinding analytic;analytic.enabled=true;analytic.analytic=AnalyticFogSettings{};
 analytic.analytic->aerial=ExponentialAerialFog{};analytic.analytic->aerial->density=.1F;
 const nlohmann::json analyticJson=analytic;check(analyticJson["schemaVersion"]==2,"Analytic parameters use binding version two");
 const auto analyticRestored=analyticJson.get<ViewFogBinding>();
 check(analyticRestored.asset.empty()&&!analyticRestored.captureView,"Analytic-only fog owns no captured resource");
 check(analyticRestored.match(moved,1)==FogViewMatch::AnalyticOnly,"World-space atmosphere accepts the current camera");
 auto combined=binding;combined.analytic=analytic.analytic;check(combined.match(moved,1)==FogViewMatch::CameraMismatch,"Volume validity remains independently observable");
 auto v2=analyticJson;v2["schemaVersion"]=3;reject([&]{(void)v2.get<ViewFogBinding>();});
 v2=analyticJson;v2["schemaVersion"]=std::uint64_t{4294967298ULL};reject([&]{(void)v2.get<ViewFogBinding>();});
 v2=analyticJson;v2["analytic"]["aerial"]["density"]=-1;reject([&]{(void)v2.get<ViewFogBinding>();});
 v2=analyticJson;v2["analytic"]["extra"]=true;reject([&]{(void)v2.get<ViewFogBinding>();});
 v2=analyticJson;v2.erase("analytic");reject([&]{(void)v2.get<ViewFogBinding>();});
 AnalyticFogSettings heightOnly;heightOnly.height=LayeredHeightFog{};
 heightOnly.height->layers.push_back({std::log(2.F)/5,0,0,{.2F,.4F,.6F}});
 heightOnly.height->startViewDepth=2;heightOnly.height->minimumTransmittance=.6F;heightOnly.height->transmittanceBias={.1F,0,0,0};
 const auto transport=evaluateAnalyticFog(heightOnly,{0,0,0},{0,0,-7},{0,0,-1});
 check(std::abs(transport.transmittance[0]-.7F)<2e-6F&&std::abs(transport.scattering[2]-.18F)<2e-6F,"Height opacity control preserves source radiance");
 analytic.analytic=heightOnly;const auto heightJson=nlohmann::json(analytic);check(heightJson.get<ViewFogBinding>().analytic->height->layers.size()==1,"Height controls roundtrip through the same binding");
 v2=heightJson;v2["analytic"]["height"]["layers"].push_back(v2["analytic"]["height"]["layers"][0]);v2["analytic"]["height"]["layers"].push_back(v2["analytic"]["height"]["layers"][0]);reject([&]{(void)v2.get<ViewFogBinding>();});
 std::cout<<"View fog binding checks: "<<checks<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
