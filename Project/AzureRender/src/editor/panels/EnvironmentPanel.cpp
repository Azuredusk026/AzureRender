#include "ui/localization/Localization.hpp"
#include "editor/ImGuiEditorLayer.hpp"
#include "ui/Widgets.hpp"
#ifdef AZURERENDER_HAS_IMGUI
namespace azurerender {
void ImGuiEditorLayer::drawEnvironmentPanel(PanelContext& context) {
    if(!ImGui::Begin(ui::tr("环境###environment"),workspace_.open("environment"))){ImGui::End();return;}
    const auto& view=context.view();
    ImGui::TextUnformatted(ui::tr("Scene rendering and presentation"));
    ImGui::BeginDisabled(session_->playing()||session_->building());
    const RenderSettings& settings = view.renderSettings();
    int skyMode=static_cast<int>(settings.sky.mode);
    if(ui::combo(ui::tr("天空背景###sky-mode"),&skyMode,"程序天空\0环境贴图\0纯色\0"))
        session_->edit("render.settings",{{"values",{{"skyMode",skyMode}}}},"sky-mode");
    if(settings.sky.mode==SkyMode::Solid){
        auto color=settings.sky.color;
        if(ImGui::ColorEdit3(ui::tr("背景颜色###sky-color"),color.data()))
            session_->edit("render.settings",{{"values",{{"skyColor",color}}}},"sky-color");
    }
    float intensity=settings.sky.intensity;
    if(ImGui::SliderFloat(ui::tr("天空亮度###sky-intensity"),&intensity,0.F,8.F))
        session_->edit("render.settings",{{"values",{{"skyIntensity",intensity}}}},"sky-intensity");
    if(settings.sky.mode!=SkyMode::Solid){
        float rotation=settings.sky.rotationDegrees;
        if(ImGui::SliderFloat(ui::tr("天空朝向###sky-rotation"),&rotation,-180.F,180.F,"%.0f"))
            session_->edit("render.settings",{{"values",{{"skyRotation",rotation}}}},"sky-rotation");
    }
    float outline = settings.outline.strength;
    if (ImGui::SliderFloat(ui::tr("Outline"), &outline, 0.0F, 2.0F)) {
        session_->edit("render.settings",{{"values",{{"outline",outline}}}},"render-outline");
    }
    float shadowRadius = settings.shadow.maximumFilterRadiusTexels;
    if (ImGui::SliderFloat(
            ui::tr("Shadow Softness"), &shadowRadius, 1.0F, 16.0F, "%.1f texels")) {
        session_->edit("render.settings",{{"values",{{"shadowRadius",shadowRadius}}}},"render-shadowRadius");
    }
    int aa=static_cast<int>(settings.antiAliasing);
    if(ui::combo(ui::tr("Anti-aliasing"),&aa,"Off\0Edge adaptive\0Supersampling 2x\0"))
        session_->edit("render.settings",{{"values",{{"antiAliasing",aa}}}},"render-aa");
    float exposure = settings.grade.exposureEv;
    if (ImGui::SliderFloat(ui::tr("Exposure EV"), &exposure, -8.0F, 8.0F)) {
        session_->edit("render.settings",{{"values",{{"exposure",exposure}}}},"render-exposure");
    }
    ImGui::EndDisabled();ImGui::End();
}
}
#endif
