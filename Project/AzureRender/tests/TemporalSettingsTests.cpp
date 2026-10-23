#include "render/RenderSettings.hpp"
#include "runtime/LevelRenderSettings.hpp"
#include "foundation/SettingRegistry.hpp"
#include "render/SpatialAntiAliasing.hpp"
#include <iostream>
#include <stdexcept>
using namespace azurerender;
void checkTemporalSetting(bool value,const char* why){if(!value)throw std::runtime_error(why);}
template<class F>void rejectTemporalSetting(F f){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}checkTemporalSetting(rejected,"Invalid temporal setting must be rejected");}
int main()try{
    checkTemporalSetting(RenderSettings::kSchemaVersion==18,"Temporal AA mode requires render settings version 18");
    RenderSettings settings;checkTemporalSetting(settings.antiAliasing==1,"Spatial AA remains the authored default");
    for(int mode=0;mode<3;++mode)checkTemporalSetting(spatialAntiAliasingMode(mode,true)==mode,"Spatial quality modes retain their mapping");
    checkTemporalSetting(spatialAntiAliasingMode(3,true)==0&&spatialAntiAliasingMode(3,false)==1,"Temporal success and unavailable history use explicit output paths");
    for(unsigned mode=0;mode<=3;++mode){settings.antiAliasing=mode;validateRenderSettings(settings);}
    settings.antiAliasing=4;rejectTemporalSetting([&]{validateRenderSettings(settings);});settings.antiAliasing=3;
    rejectTemporalSetting([&]{(void)migrateRenderSettings(settings,17);});
    checkTemporalSetting(migrateRenderSettings(settings,18).antiAliasing==3,"Current settings version accepts temporal mode");
    const auto encoded=encodeLevelRenderSettings(settings);
    checkTemporalSetting(encoded.at("schemaVersion")==18&&encoded.at("antiAliasing")==3,"Full JSON settings declare their version");
    RenderSettings roundtrip;decodeLevelRenderSettings(roundtrip,encoded);checkTemporalSetting(roundtrip.antiAliasing==3,"Temporal mode roundtrips through the shared contract");
    rejectTemporalSetting([&]{decodeLevelRenderSettings(roundtrip,{{"antiAliasing",3}});});
    rejectTemporalSetting([&]{decodeLevelRenderSettings(roundtrip,{{"schemaVersion",17},{"antiAliasing",3}});});
    for(const auto& version:{nlohmann::json(19),nlohmann::json(18.5),nlohmann::json(-1),nlohmann::json(true)})
        rejectTemporalSetting([&]{decodeLevelRenderSettings(roundtrip,{{"schemaVersion",version},{"antiAliasing",3}});});
    decodeLevelRenderSettings(roundtrip,{{"exposure",.5}});
    checkTemporalSetting(roundtrip.antiAliasing==3,"An older partial overlay can inherit the current temporal mode");
    for(unsigned mode=0;mode<3;++mode){RenderSettings legacy;decodeLevelRenderSettings(legacy,{{"antiAliasing",mode}});checkTemporalSetting(legacy.antiAliasing==mode,"Legacy mode meanings are preserved");}
    SettingRegistry registry;registerRenderSettings(registry);const auto description=registry.describe("render.antiAliasing");
    checkTemporalSetting(description.at("render.antiAliasing").at("maximum")==3&&description.at("render.antiAliasing").at("default")==1,
        "Console and UI metadata share the current range and existing default");
    checkTemporalSetting(registry.set("render.antiAliasing",3,SettingSource::Console).passed,"Console accepts temporal AA");
    checkTemporalSetting(!registry.set("render.antiAliasing",4,SettingSource::Console).passed,"Console rejects unknown AA modes");
    std::cout<<"Temporal settings version, migration, overlays and metadata passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
