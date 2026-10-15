#include "editor/ImGuiEditorLayer.hpp"
#include "ui/Widgets.hpp"
#ifdef AZURERENDER_HAS_IMGUI
namespace azurerender {
void ImGuiEditorLayer::drawEnvironmentPanel(PanelContext& context) {
    if(!ImGui::Begin("环境###environment",workspace_.open("environment"))){ImGui::End();return;}
    const auto& view=context.view();
    ImGui::TextUnformatted("Scene rendering and presentation");
    ImGui::BeginDisabled(session_->playing()||session_->building());
    const RenderSettings& settings = view.renderSettings();
    int skyMode=static_cast<int>(settings.sky.mode);
    if(ImGui::Combo("天空背景###sky-mode",&skyMode,"程序天空\0环境贴图\0纯色\0"))
        session_->edit("render.settings",{{"values",{{"skyMode",skyMode}}}},"sky-mode");
    if(settings.sky.mode==SkyMode::Solid){
        auto color=settings.sky.color;
        if(ImGui::ColorEdit3("背景颜色###sky-color",color.data()))
            session_->edit("render.settings",{{"values",{{"skyColor",color}}}},"sky-color");
    }
    float intensity=settings.sky.intensity;
    if(ImGui::SliderFloat("天空亮度###sky-intensity",&intensity,0.F,8.F))
        session_->edit("render.settings",{{"values",{{"skyIntensity",intensity}}}},"sky-intensity");
    if(settings.sky.mode!=SkyMode::Solid){
        float rotation=settings.sky.rotationDegrees;
        if(ImGui::SliderFloat("天空朝向###sky-rotation",&rotation,-180.F,180.F,"%.0f"))
            session_->edit("render.settings",{{"values",{{"skyRotation",rotation}}}},"sky-rotation");
    }
    float outline = settings.outline.strength;
    if (ImGui::SliderFloat("Outline", &outline, 0.0F, 2.0F)) {
        session_->edit("render.settings",{{"values",{{"outline",outline}}}},"render-outline");
    }
    float shadowRadius = settings.shadow.maximumFilterRadiusTexels;
    if (ImGui::SliderFloat(
            "Shadow Softness", &shadowRadius, 1.0F, 16.0F, "%.1f texels")) {
        session_->edit("render.settings",{{"values",{{"shadowRadius",shadowRadius}}}},"render-shadowRadius");
    }
    int aa=static_cast<int>(settings.antiAliasing);
    if(ImGui::Combo("Anti-aliasing",&aa,"Off\0Edge adaptive\0Supersampling 2x\0"))
        session_->edit("render.settings",{{"values",{{"antiAliasing",aa}}}},"render-aa");
    float exposure = settings.grade.exposureEv;
    if (ImGui::SliderFloat("Exposure EV", &exposure, -8.0F, 8.0F)) {
        session_->edit("render.settings",{{"values",{{"exposure",exposure}}}},"render-exposure");
    }
    ImGui::EndDisabled();ImGui::End();
}
}
#endif
