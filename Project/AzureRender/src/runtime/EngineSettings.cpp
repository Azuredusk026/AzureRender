#include "EngineSettings.hpp"
#include "InputPreferences.hpp"
#include "extensions/BuiltinRenderFeatures.hpp"
namespace azurerender {
void registerEngineSettings(SettingRegistry& r) {
    registerRenderSettings(r);registerInputPreferences(r);
    for(const auto& descriptor:builtinRenderFeatures().descriptors())
        r.add({"render.features."+descriptor.id,"可选渲染特性："+descriptor.id,false,{},{},false,true,false,false});
    r.add({"runtime.maxFrameRate","Maximum host frame rate (0 is unlimited)",60,0,1000,false,true,false});
    r.add({"runtime.vsync","Synchronized surface presentation",true,{},{},false,true,true});
    r.add({"diagnostics.verbose","Verbose development diagnostics",false,{},{},false,true,false});
    r.add({"developer.maxViews","Maximum owned auxiliary render views",3,1,3,false,true,true});
}
void RenderSettingOverlay::apply(const SettingRegistry& registry,RenderSettings& settings) {
    using Json=nlohmann::json;
    const auto apply=[&](const std::string& name,auto read,auto write) {
        const Json value=read();auto found=active_.find(name);
        if(found!=active_.end()&&value!=found->second.last)found->second.baseline=value;
        if(registry.source(name)==SettingSource::Default) {
            if(found!=active_.end()){write(found->second.baseline);active_.erase(found);}return;
        }
        if(found==active_.end())found=active_.emplace(name,Active{value,value}).first;
        write(registry.get(name));found->second.last=read();
    };
    apply("render.shadowDistance",[&]()->Json{return settings.shadowDistance;},[&](const Json& v){settings.shadowDistance=v.get<float>();});
    apply("render.antiAliasing",[&]()->Json{return settings.antiAliasing;},[&](const Json& v){settings.antiAliasing=v.get<unsigned>();});
    apply("render.diagnosticView",[&]()->Json{return settings.diagnosticView;},[&](const Json& v){settings.diagnosticView=v.get<unsigned>();});
    apply("render.exposure",[&]()->Json{return settings.grade.exposureEv;},[&](const Json& v){settings.grade.exposureEv=v.get<float>();});
    apply("render.blackholeQuality",[&]()->Json{return static_cast<int>(settings.blackhole.quality);},[&](const Json& v){settings.blackhole.quality=static_cast<BlackholeQuality>(v.get<int>());});
    validateRenderSettings(settings);
}
RenderSettings resolveRenderSettings(const SettingRegistry& registry,RenderSettings authored) {
    RenderSettingOverlay overlay;overlay.apply(registry,authored);
    const auto enabled=[&](const std::string& id){const auto key="render.features."+id;return registry.source(key)==SettingSource::Default?authored.sceneType==SceneType::Character:registry.get(key).get<bool>();};
    authored.faceSdfFeature=enabled("face-sdf");authored.showcasePlatformFeature=enabled("showcase-platform");
    authored.faceSdf.enabled=authored.faceSdf.enabled&&authored.faceSdfFeature;
    authored.characterPresentation.platformEnabled=authored.characterPresentation.platformEnabled&&authored.showcasePlatformFeature;
    return authored;
}
}
