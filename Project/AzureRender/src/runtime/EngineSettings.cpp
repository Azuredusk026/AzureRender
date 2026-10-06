#include "EngineSettings.hpp"
#include "InputPreferences.hpp"
namespace azurerender {
void registerEngineSettings(SettingRegistry& r) {
    registerRenderSettings(r);registerInputPreferences(r);
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
    apply("render.diagnosticView",[&]()->Json{return settings.diagnosticView;},[&](const Json& v){settings.diagnosticView=v.get<unsigned>();});
    apply("render.exposure",[&]()->Json{return settings.grade.exposureEv;},[&](const Json& v){settings.grade.exposureEv=v.get<float>();});
    apply("render.blackholeQuality",[&]()->Json{return static_cast<int>(settings.blackhole.quality);},[&](const Json& v){settings.blackhole.quality=static_cast<BlackholeQuality>(v.get<int>());});
    validateRenderSettings(settings);
}
RenderSettings resolveRenderSettings(const SettingRegistry& registry,RenderSettings authored) {
    RenderSettingOverlay overlay;overlay.apply(registry,authored);return authored;
}
}
