#include "render/ScreenSpaceReflection.hpp"
#include "runtime/LevelRenderSettings.hpp"
#include "runtime/SceneDocument.hpp"
#include <iostream>
#include <limits>
#include <functional>
#include <cmath>
using namespace azurerender;
void check(bool p,const char* label){if(!p)throw std::runtime_error(label);}
void rejects(const std::function<void()>& f,const char* label){try{f();}catch(const std::exception&){return;}throw std::runtime_error(label);}
int main(int argc,char** argv){
 RenderSettings settings;
 check(!settings.screenSpaceReflection.enabled,"default disabled");
 check(settings.screenSpaceReflection.algorithm==ScreenSpaceReflectionAlgorithm::Linear,"default linear algorithm");
 settings.screenSpaceReflection.enabled=true;
 settings.screenSpaceReflection.maxDistance=31.125F;
 settings.screenSpaceReflection.thickness=.075F;
 settings.screenSpaceReflection.maxSteps=128;
 settings.screenSpaceReflection.algorithm=ScreenSpaceReflectionAlgorithm::Projected;
 auto encoded=encodeLevelRenderSettings(settings);
 check(encoded["screenSpaceReflection"]["schemaVersion"]==2&&encoded["screenSpaceReflection"]["algorithm"]=="projected","algorithm has a versioned serialized name");
 RenderSettings reopened;decodeLevelRenderSettings(reopened,encoded);
 check(reopened.screenSpaceReflection.enabled&&reopened.screenSpaceReflection.maxDistance==31.125F&&reopened.screenSpaceReflection.maxSteps==128,"level roundtrip");
 check(reopened.screenSpaceReflection.algorithm==ScreenSpaceReflectionAlgorithm::Projected,"projected algorithm roundtrip");
 auto legacy=encoded;legacy["screenSpaceReflection"]["schemaVersion"]=1;legacy["screenSpaceReflection"].erase("algorithm");
 RenderSettings old=reopened;decodeLevelRenderSettings(old,legacy);check(old.screenSpaceReflection.algorithm==ScreenSpaceReflectionAlgorithm::Linear,"version-one data must select linear even with projected target defaults");
 for(const auto& mutation:{nlohmann::json{{"schemaVersion",3}},nlohmann::json{{"schemaVersion",1}},nlohmann::json{{"algorithm","unknown"}},nlohmann::json{{"algorithm",1}},nlohmann::json{{"maxSteps",-1}},nlohmann::json{{"maxSteps",16.5}},nlohmann::json{{"thickness",0}},nlohmann::json{{"normalBias",std::numeric_limits<double>::infinity()}},nlohmann::json{{"unknown",1}}}){
  auto bad=encoded;bad["screenSpaceReflection"].update(mutation);
  rejects([&]{decodeLevelRenderSettings(reopened,bad);},"reject invalid config");
  check(reopened.screenSpaceReflection.maxDistance==31.125F,"transactional decode");
  check(reopened.screenSpaceReflection.algorithm==ScreenSpaceReflectionAlgorithm::Projected,"invalid algorithm decode must preserve target");
 }
 check(!migrateRenderSettings(settings,15).screenSpaceReflection.enabled,"schema15 default disabled");
 rejects([&]{(void)migrateRenderSettings(settings,RenderSettings::kSchemaVersion+1);},"reject future schema");
 check(screenSpaceReflectionActive(settings.screenSpaceReflection,true,0,true),"beauty active");
 check(screenSpaceReflectionActive(settings.screenSpaceReflection,true,6,true),"specular isolation active");
 check(screenSpaceReflectionActive(settings.screenSpaceReflection,true,20,true),"environment isolation active");
 check(!screenSpaceReflectionActive(settings.screenSpaceReflection,true,1,true),"albedo excludes extra snapshot");
 check(!screenSpaceReflectionActive(settings.screenSpaceReflection,true,0,false),"disabled specular excludes extra snapshot");
 check(!screenSpaceReflectionActive(settings.screenSpaceReflection,false,0,true),"showcase excluded");
 if(argc>1){std::filesystem::create_directories(std::filesystem::path(argv[1]).parent_path());SceneDocument document;document.sceneId="ssr-cpu-fixture";document.renderSettings=settings;document.save(argv[1]);
  const auto loaded=SceneDocument::load(argv[1]);check(loaded.renderSettings.screenSpaceReflection.maxDistance==31.125F&&loaded.renderSettings.screenSpaceReflection.enabled&&loaded.renderSettings.screenSpaceReflection.algorithm==ScreenSpaceReflectionAlgorithm::Projected,"scene save reopen");}
 std::cout<<"PASS: default, legal domain, version, atomic decode, level JSON, azscene save/reopen, snapshot activation\n";
}
