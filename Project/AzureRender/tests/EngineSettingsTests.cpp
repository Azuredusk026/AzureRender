#include "runtime/EngineSettings.hpp"
#include "runtime/LevelRenderSettings.hpp"
#include "runtime/SceneDocument.hpp"
#include <chrono>
#include "foundation/FramePacing.hpp"
#include <cmath>
#include <limits>
#include <iostream>
using namespace azurerender;
void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
int main(){try{
    RenderSettings styled;
    decodeLevelRenderSettings(styled,{{"nprLighting",{{"schemaVersion",1},{"enabled",true},
        {"ambientColor",{1.2,.9,.8}},{"ambientDirection",{0,1,0}},{"directionBias",.15},
        {"directionGain",1.5},{"directionFloor",.5},{"outdoorGain",1.05},{"shadowAmbientGain",.8},
        {"indoorMix",0},{"litAmbientMix",1},{"mainColorMix",1},{"shadowOverride",1},{"useSceneIrradiance",false}}}});
    const auto styledJson=encodeLevelRenderSettings(styled);
    check(styledJson.contains("nprLighting")&&styledJson.at("nprLighting").at("enabled")==true,
        "NPR illumination must roundtrip through the shared scene settings contract");
    for(const auto& mutation:{nlohmann::json{{"schemaVersion",1.5}},nlohmann::json{{"enabled",1}},
        nlohmann::json{{"directionGain",true}},nlohmann::json{{"ambientDirection",{0,0,0}}},
        nlohmann::json{{"ambientColor",{1,2}}},nlohmann::json{{"mainColorMix",-1}},nlohmann::json{{"unknown",1}}}){
        auto invalid=styledJson;invalid["nprLighting"].update(mutation);bool rejected=false;
        try{RenderSettings value;decodeLevelRenderSettings(value,invalid);}catch(const std::exception&){rejected=true;}
        check(rejected,"Malformed NPR illumination must be rejected");
    }
    auto styleScene=SceneDocument::fromAsset("test.gltf");styleScene.renderSettings=styled;
    const auto styleFile=std::filesystem::temp_directory_path()/("azure-npr-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".azscene");
    styleScene.save(styleFile);auto styleReopened=SceneDocument::load(styleFile);std::filesystem::remove(styleFile);
    check(encodeNprLighting(styleReopened.renderSettings.nprLighting)==styledJson.at("nprLighting"),"Standalone scene must preserve NPR illumination");
    check(!migrateRenderSettings(styled,16).nprLighting.enabled,"Earlier settings versions retain their compatible NPR mode");
    RenderSettings lutDefaults;
    const auto lutJson=encodeLevelRenderSettings(lutDefaults);
    check(lutJson.contains("gradeLut")&&lutJson.at("gradeLut").at("asset")=="",
        "Color grading LUT configuration must persist through the public level settings contract");
    RenderSettings tracing;
    decodeLevelRenderSettings(tracing,{{"renderer","pathtracing"},{"pathTraceSamples",64},{"pathTraceBounces",6}});
    const auto savedTracing=encodeLevelRenderSettings(tracing);
    check(savedTracing.contains("renderer")&&savedTracing.at("renderer")=="pathtracing","Path tracing mode persists through scene settings");
    RenderSettings sky;
    decodeLevelRenderSettings(sky,{{"skyMode",2},{"skyColor",{.1F,.2F,.3F}},{"skyIntensity",1.5F},{"skyRotation",90.F}});
    const auto savedSky=encodeLevelRenderSettings(sky);
    check(savedSky.contains("skyMode")&&savedSky.at("skyMode")==2,"Sky mode persists through scene settings");
    check(savedSky.at("skyColor")==nlohmann::json({.1F,.2F,.3F}),"Solid sky color persists");
    for(const auto& invalid:{nlohmann::json{{"skyMode",3}},nlohmann::json{{"skyIntensity",-1}},nlohmann::json{{"skyColor",{1,2}}}}){
        bool rejected=false;try{decodeLevelRenderSettings(sky,invalid);}catch(const std::exception&){rejected=true;}
        check(rejected&&encodeLevelRenderSettings(sky)==savedSky,"Invalid sky edit is rejected atomically");
    }
    auto skyScene=SceneDocument::fromAsset("test.gltf");skyScene.renderSettings=sky;
    const auto skyFile=std::filesystem::temp_directory_path()/("azure-sky-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".azscene");
    skyScene.save(skyFile);const auto reopenedSky=SceneDocument::load(skyFile);std::filesystem::remove(skyFile);
    check(reopenedSky.renderSettings.sky.mode==sky.sky.mode&&reopenedSky.renderSettings.sky.color==sky.sky.color,"Standalone scene sky survives save and reopen");
    SettingRegistry registry;registerEngineSettings(registry);RenderSettings render;render.grade.exposureEv=1.5F;
    check(!registry.describe().at("render.features.face-sdf").value("editorVisible",true),"Extension controls are code-owned rather than generic editor settings");
    check(registry.describe().at("render.exposure").value("editorVisible",false),"General rendering controls remain available");
    RenderSettings basic;basic.sceneType=SceneType::Sample;
    check(!resolveRenderSettings(registry,basic).faceSdf.enabled,"Generic scenes must not activate character face shading by default");
    check(!resolveRenderSettings(registry,basic).characterPresentation.platformEnabled,"Generic scenes must not generate showcase platforms");
    check(resolveRenderSettings(registry,render).faceSdf.enabled,"Legacy character scenes retain their declared face behavior");
    check(registry.set("render.features.face-sdf",false,SettingSource::Project).passed,"Project can disable a render feature");registry.applyPending();
    check(!resolveRenderSettings(registry,render).faceSdf.enabled,"Explicit project disable wins over legacy selection");
    check(!registry.set("render.features.unknown",true,SettingSource::Project).passed,"Unknown features produce a typed settings diagnostic");
    check(registry.get("runtime.vsync")==true,"Default synchronized presentation is explicit");
    check(registry.set("runtime.vsync",false,SettingSource::CommandLine).passed,"Startup presentation override");
    registry.applyPending();check(registry.get("runtime.vsync")==false,"Startup override reaches the shared setting");
    check(!registry.set("runtime.vsync",1,SettingSource::Console).passed,"Presentation setting requires a boolean");
    registry.start();
    check(!registry.set("runtime.vsync",true,SettingSource::Console).passed,"Presentation changes require a new host session");
    check(registry.set("runtime.maxFrameRate",100,SettingSource::Console).passed,"Frame rate is a shared runtime setting");
    registry.applyPending();
    check(std::abs(framePacingWaitSeconds(.005,registry.get("runtime.maxFrameRate").get<unsigned>())-.005)<1e-12,"Wait only for the remaining frame budget");
    check(framePacingWaitSeconds(.02,60)==0,"Slow frames are never delayed or caught up by skipping work");
    check(framePacingWaitSeconds(.005,0)==0,"Unlimited mode has no pacing wait");
    check(std::abs(framePacingWaitSeconds(0,60)-1.0/60)<1e-12,"An empty frame uses its complete time budget");
    check(!registry.set("runtime.maxFrameRate",-1,SettingSource::Console).passed,"Negative rate is rejected");
    check(!registry.set("runtime.maxFrameRate",1001,SettingSource::Console).passed,"Out of range rate is rejected");
    check(!registry.set("runtime.maxFrameRate",60.5,SettingSource::Console).passed,"Fractional frame rates are rejected");
    check(registry.set("runtime.maxFrameRate",0,SettingSource::Console).passed,"Unlimited setting is valid");
    registry.applyPending();check(framePacingWaitSeconds(.005,registry.get("runtime.maxFrameRate").get<unsigned>())==0,"Runtime changes reach the pacing contract");
    check(registry.reset("runtime.maxFrameRate",SettingSource::Console).passed,"Frame rate can be reset");registry.applyPending();
    check(framePacingWaitSeconds(.005,registry.get("runtime.maxFrameRate").get<unsigned>())>0,"Reset restores the configured pacing policy");
    for(double elapsed:{-1.0,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}) {
        bool rejected=false;try{static_cast<void>(framePacingWaitSeconds(elapsed,60));}catch(const std::invalid_argument&){rejected=true;}
        check(rejected,"Invalid elapsed time cannot become a sleep duration");
    }
    RenderSettingOverlay overlay;overlay.apply(registry,render);
    check(render.grade.exposureEv==1.5F,"Project rendering retained without an override");
    check(registry.set("render.exposure",2.0,SettingSource::Console).passed,"Validated render override");registry.applyPending();overlay.apply(registry,render);
    check(render.grade.exposureEv==2.F,"Boundary affects production RenderSettings");
    registry.reset("render.exposure",SettingSource::Console);registry.applyPending();overlay.apply(registry,render);
    check(render.grade.exposureEv==1.5F,"Reset restores project rendering");
    check(registry.set("render.diagnosticView",4,SettingSource::CommandLine).passed,"Command setting");registry.applyPending();overlay.apply(registry,render);
    check(render.diagnosticView==4,"Typed diagnostic applies");
    RenderSettings authored;authored.grade.exposureEv=1.5F;
    registry.set("render.exposure",2.0,SettingSource::Console);registry.applyPending();
    auto effective=resolveRenderSettings(registry,authored);
    check(authored.grade.exposureEv==1.5F&&effective.grade.exposureEv==2.F,"Rendering snapshot preserves authoring state");
    registry.reset("render.exposure",SettingSource::Console);registry.applyPending();
    effective=resolveRenderSettings(registry,authored);check(effective.grade.exposureEv==1.5F,"Fresh snapshot reset uses authored value");
    check(!registry.replaceLayer({{"render.exposure",0.0},{"render.diagnosticView",5}},SettingSource::Project).passed,"Layer validates all before applying");
    registry.applyPending();check(registry.get("render.exposure")==0.0,"Failed project layer keeps defaults");
    std::cout<<"Rendering overlay, source and reset passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
