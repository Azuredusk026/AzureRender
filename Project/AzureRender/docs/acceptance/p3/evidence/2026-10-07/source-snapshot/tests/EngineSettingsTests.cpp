#include "runtime/EngineSettings.hpp"
#include <iostream>
using namespace azurerender;
void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
int main(){try{
    SettingRegistry registry;registerEngineSettings(registry);RenderSettings render;render.grade.exposureEv=1.5F;
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
