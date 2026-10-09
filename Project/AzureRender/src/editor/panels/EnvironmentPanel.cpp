#include "editor/ImGuiEditorLayer.hpp"
#include "editor/ui/Widgets.hpp"
#ifdef AZURERENDER_HAS_IMGUI
namespace azurerender {
void ImGuiEditorLayer::drawEnvironmentPanel(PanelContext& context) {
    if(!ImGui::Begin("Environment###environment",workspace_.open("environment"))){ImGui::End();return;}
    const auto& view=context.view();
    ImGui::TextUnformatted("Scene rendering and presentation");
    ImGui::BeginDisabled(session_->playing()||session_->building());
    const RenderSettings& settings = view.renderSettings();
    if (ImGui::BeginCombo(
            "Showcase Look",
            std::string(showcasePresetName(settings.showcasePreset)).c_str())) {
        for (std::uint32_t preset = 0; preset < 5; ++preset) {
            const bool selected = settings.showcasePreset == preset;
            const std::string name(showcasePresetName(preset));
            if (ImGui::Selectable(name.c_str(), selected)) {
                session_->edit("render.preset",{{"value",preset}});
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    bool background = settings.characterPresentation.backgroundEnabled;
    if (ImGui::Checkbox("Background", &background)) {
        session_->edit("render.settings",{{"values",{{"background",background}}}},"render-background");
    }
    bool platform = settings.characterPresentation.platformEnabled;
    if (ImGui::Checkbox("Showcase Platform", &platform)) {
        session_->edit("render.settings",{{"values",{{"platform",platform}}}},"render-platform");
    }
    bool faceSdf = settings.faceSdf.enabled;
    if (ImGui::Checkbox("Face SDF", &faceSdf)) {
        session_->edit("render.settings",{{"values",{{"faceSdf",faceSdf}}}},"render-faceSdf");
    }
    float threshold = settings.faceSdf.threshold;
    if (ImGui::SliderFloat("Face SDF Threshold", &threshold, 0.0F, 1.0F)) {
        session_->edit("render.settings",{{"values",{{"faceThreshold",threshold}}}},"render-faceThreshold");
    }
    float softness = settings.faceSdf.softness;
    if (ImGui::SliderFloat("Face SDF Softness", &softness, 0.001F, 0.5F)) {
        session_->edit("render.settings",{{"values",{{"faceSoftness",softness}}}},"render-faceSoftness");
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
