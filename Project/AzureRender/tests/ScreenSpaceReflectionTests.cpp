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
 settings.screenSpaceReflection.enabled=true;
 settings.screenSpaceReflection.maxDistance=31.125F;
 settings.screenSpaceReflection.thickness=.075F;
 settings.screenSpaceReflection.maxSteps=128;
 auto encoded=encodeLevelRenderSettings(settings);
 RenderSettings reopened;decodeLevelRenderSettings(reopened,encoded);
 check(reopened.screenSpaceReflection.enabled&&reopened.screenSpaceReflection.maxDistance==31.125F&&reopened.screenSpaceReflection.maxSteps==128,"level roundtrip");
 for(const auto& mutation:{nlohmann::json{{"schemaVersion",2}},nlohmann::json{{"maxSteps",-1}},nlohmann::json{{"maxSteps",16.5}},nlohmann::json{{"thickness",0}},nlohmann::json{{"normalBias",std::numeric_limits<double>::infinity()}},nlohmann::json{{"unknown",1}}}){
  auto bad=encoded;bad["screenSpaceReflection"].update(mutation);
  rejects([&]{decodeLevelRenderSettings(reopened,bad);},"reject invalid config");
  check(reopened.screenSpaceReflection.maxDistance==31.125F,"transactional decode");
 }
 check(!migrateRenderSettings(settings,15).screenSpaceReflection.enabled,"schema15 default disabled");
 rejects([&]{(void)migrateRenderSettings(settings,17);},"reject future schema");
 check(screenSpaceReflectionActive(settings.screenSpaceReflection,true,0,true),"beauty active");
 check(screenSpaceReflectionActive(settings.screenSpaceReflection,true,6,true),"specular isolation active");
 check(screenSpaceReflectionActive(settings.screenSpaceReflection,true,20,true),"environment isolation active");
 check(!screenSpaceReflectionActive(settings.screenSpaceReflection,true,1,true),"albedo excludes extra snapshot");
 check(!screenSpaceReflectionActive(settings.screenSpaceReflection,true,0,false),"disabled specular excludes extra snapshot");
 check(!screenSpaceReflectionActive(settings.screenSpaceReflection,false,0,true),"showcase excluded");
 if(argc>1){std::filesystem::create_directories(std::filesystem::path(argv[1]).parent_path());SceneDocument document;document.sceneId="ssr-cpu-fixture";document.renderSettings=settings;document.save(argv[1]);
  const auto loaded=SceneDocument::load(argv[1]);check(loaded.renderSettings.screenSpaceReflection.maxDistance==31.125F&&loaded.renderSettings.screenSpaceReflection.enabled,"scene save reopen");}
 std::cout<<"PASS: default, legal domain, version, atomic decode, level JSON, azscene save/reopen, snapshot activation\n";
}
