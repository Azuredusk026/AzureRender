#include "editor/ImGuiEditorLayer.hpp"
#include "ui/Widgets.hpp"
#ifdef AZURERENDER_HAS_IMGUI
namespace azurerender {
void ImGuiEditorLayer::drawEnvironmentPanel(PanelContext& context) {
    if(!ImGui::Begin("Environment###environment",workspace_.open("environment"))){ImGui::End();return;}
    const auto& view=context.view();
    ImGui::TextUnformatted("Scene rendering and presentation");
    ImGui::BeginDisabled(session_->playing()||session_->building());
    const RenderSettings& settings = view.renderSettings();
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
