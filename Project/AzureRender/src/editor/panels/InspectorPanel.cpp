#include "editor/ImGuiEditorLayer.hpp"
#include "editor/ui/Widgets.hpp"
#include "editor/ui/UiScopes.hpp"
#include "reflection/Registry.hpp"
#include "runtime/ComponentRegistry.hpp"
#include "diagnostics/RuntimeDiagnostics.hpp"
#ifdef AZURERENDER_HAS_IMGUI
#include <imgui.h>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <map>
#include <set>
namespace azurerender {
#ifndef IMGUI_HAS_DOCK
namespace {
void setFallbackPanelRect(float x,float y,float width,float height) {
    const auto display=ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos({display.x*x,display.y*y},ImGuiCond_Always);
    ImGui::SetNextWindowSize({display.x*width,display.y*height},ImGuiCond_Always);
}
}
#endif
void ImGuiEditorLayer::drawInspectorPanel(PanelContext& panelContext) {
    const auto& view=panelContext.view();
#ifndef IMGUI_HAS_DOCK
    setFallbackPanelRect(0.76F, 0.0F, 0.24F, 0.72F);
#endif
    if(!ImGui::Begin("Details###inspector",workspace_.open("inspector"))){ImGui::End();return;}
    ImGui::BeginDisabled(session_->playing() || session_->building());
    const auto selected=view.selectedNode()?std::optional<SceneNode>(*view.selectedNode()):std::nullopt;
    if (selected) {
        const auto* node=&*selected;
        ImGui::Text("Node: %s", node->name.c_str());
        ImGui::Text("Id: %s", node->id.c_str());
        bool visible = node->visible;
        if (ImGui::Checkbox("Visible", &visible)) {
            session_->edit("node.visible",{{"value",visible}});
        }
        ImGui::TextUnformatted("Name");
        std::array<char, 128> nameBuffer{};
        const std::size_t copyLength = std::min(
            node->name.size(), nameBuffer.size() - 1);
        std::memcpy(
            nameBuffer.data(), node->name.data(), copyLength);
        if (ImGui::InputText("##name", nameBuffer.data(), nameBuffer.size())) {
            session_->edit("node.rename",{{"value",nameBuffer.data()}},"details-name");
        }
        observeWidget("name");
        ImGui::TextUnformatted("Prefab Source");
        std::array<char, 256> prefabBuffer{};
        std::memcpy(prefabBuffer.data(), node->prefabSource.data(),
            std::min(node->prefabSource.size(), prefabBuffer.size() - 1));
        if (ImGui::InputText("##prefab", prefabBuffer.data(), prefabBuffer.size())) {
            session_->edit("node.prefab-source",{{"value",prefabBuffer.data()}},"details-prefab");
        }
        ImGui::TextUnformatted("Instance Of");
        std::array<char, 128> instanceBuffer{};
        std::memcpy(instanceBuffer.data(), node->instanceOf.data(),
            std::min(node->instanceOf.size(), instanceBuffer.size() - 1));
        if (ImGui::InputText("##instance", instanceBuffer.data(), instanceBuffer.size())) {
            session_->edit("node.instance",{{"value",instanceBuffer.data()}},"details-instance");
        }
    }
    if(view.isProject() && view.selectedNode()){
        std::vector<std::string> types;
        for(const auto& entry:runtimeComponentRegistry().metadata().types())
            if(entry.first!="azure.transform" && entry.first!="azure.renderable") types.push_back(entry.first);
        ImGui::BeginDisabled(session_->playing() || session_->building());
        if(ImGui::BeginCombo("Add Component","Choose type")){for(const auto& type:types)if(ImGui::Selectable(type.c_str()))try{session_->edit("component.add",{{"type",type}});}catch(const std::exception& error){session_->log(std::string("ERROR: ")+error.what());}ImGui::EndCombo();}
        const auto& componentRegistry=runtimeComponentRegistry().metadata();
        for(const auto& type:types){auto data=view.componentData(view.selectedNode()->id,type);if(data.is_null())continue;
            ImGui::PushID(type.c_str());if(ImGui::CollapsingHeader(type.c_str()))for(const auto& field:componentRegistry.type(type).properties){
                if(!field.toolVisible)continue;
                ImGui::BeginDisabled(field.readOnly);
                auto value=data.at(field.name);bool changed=false;
                if(value.is_boolean()){bool v=value.get<bool>();changed=ImGui::Checkbox(field.label.c_str(),&v);value=v;}
                else if(value.is_number_integer()){int v=value.get<int>();changed=ImGui::DragInt(field.label.c_str(),&v,1,static_cast<int>(field.minimum),static_cast<int>(field.maximum));value=v;}
                else if(value.is_number()){float v=value.get<float>();changed=ImGui::DragFloat(field.label.c_str(),&v,0.01F,static_cast<float>(field.minimum),static_cast<float>(field.maximum));value=v;}
                else if(value.is_array() && value.size()==3){auto v=value.get<std::array<float,3>>();changed=ImGui::DragFloat3(field.label.c_str(),v.data(),0.01F,static_cast<float>(field.minimum),static_cast<float>(field.maximum));value=v;}
                else if(value.is_string()){
                    const auto text=value.get<std::string>();
                    if(field.name=="target"){
                        if(ImGui::BeginCombo(field.label.c_str(),text.c_str())){for(const auto& node:view.scene().nodes)if(ImGui::Selectable(node.id.c_str(),node.id==text)){value=node.id;changed=true;}ImGui::EndCombo();}
                    }else if(field.name=="asset"){
                        const std::string extension=type=="azure.script"?".lua":type=="azure.animator"?".json":type=="azure.audio-source"?".wav":".rml";
                        if(ImGui::BeginCombo(field.label.c_str(),text.c_str())){for(const auto& [id,record]:view.assets().records())if(record.path.extension()==extension && ImGui::Selectable(record.virtualPath.c_str(),id==text)){value=id;changed=true;}ImGui::EndCombo();}
                    }else{
                        std::array<char,512> v{};std::memcpy(v.data(),text.data(),std::min(text.size(),v.size()-1));
                        changed=ImGui::InputText(field.label.c_str(),v.data(),v.size(),ImGuiInputTextFlags_EnterReturnsTrue);value=v.data();
                    }
                }
                ImGui::EndDisabled();
                if(!field.tooltip.empty() && ImGui::IsItemHovered())ImGui::SetTooltip("%s",field.tooltip.c_str());
                if(changed)try{session_->edit("component.field",{{"type",type},{"field",field.name},{"value",value}},"component-"+type+":"+field.name);}catch(const std::exception& error){session_->log(std::string("ERROR: ")+error.what());}
            }ImGui::PopID();
        }
        ImGui::EndDisabled();
    }
    ImGui::Separator();
    ImGui::Text("Transform | Position (m), Rotation (deg), Scale");
    if(ui::button("Reset Transform")){session_->edit("node.transform",{{"translation",{0,0,0}},{"rotation",{0,0,0}},{"scale",{1,1,1}}});}
    ImGui::SetNextItemWidth(-110*dpi_);
    static const auto registry = reflection::makeRuntimeRegistry();
    ecs::TransformComponent transform{view.gizmoTranslation(), view.gizmoRotation(), view.gizmoScale()};
    for (const auto& property : registry.type("azure.transform").properties) {
        ImGui::SetNextItemWidth(-110*dpi_);
        auto value = property.read(&transform).get<std::array<float, 3>>();
        if (ImGui::DragFloat3(property.label.c_str(), value.data(), 0.01F,
                static_cast<float>(property.minimum), static_cast<float>(property.maximum))) {
            property.write(&transform, value);
            session_->edit("node.transform",{{property.name,value}},"details-"+property.name);
        }
    }
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
    float exposure = settings.grade.exposureEv;
    if (ImGui::SliderFloat("Exposure EV", &exposure, -8.0F, 8.0F)) {
        session_->edit("render.settings",{{"values",{{"exposure",exposure}}}},"render-exposure");
    }
    ImGui::EndDisabled();
    ImGui::End();
}


}
#endif
