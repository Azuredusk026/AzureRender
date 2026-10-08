#include "editor/ImGuiEditorLayer.hpp"
#include "editor/ui/Widgets.hpp"
#include "editor/ui/UiScopes.hpp"
#include "reflection/Registry.hpp"
#include "runtime/ComponentRegistry.hpp"
#include "runtime/AssetTypeRegistry.hpp"
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
        auto name=node->name;
        ImGui::SetNextItemWidth(-1);
        if(ui::inputText("##name",name))session_->edit("node.rename",{{"value",name}},"details-name");
        if(ImGui::IsItemDeactivatedAfterEdit())session_->edit("history.end-edit");
        observeWidget("name");
        ImGui::TextUnformatted("Prefab Source");auto prefab=node->prefabSource;ImGui::SetNextItemWidth(-1);
        if(ui::inputText("##prefab",prefab,ImGuiInputTextFlags_EnterReturnsTrue))session_->edit("node.prefab-source",{{"value",prefab}},"details-prefab");
        ImGui::TextUnformatted("Instance Of");auto instance=node->instanceOf;ImGui::SetNextItemWidth(-1);
        if(ui::inputText("##instance",instance,ImGuiInputTextFlags_EnterReturnsTrue))session_->edit("node.instance",{{"value",instance}},"details-instance");
    }
    if(view.isProject() && view.selectedNode()){
        std::vector<std::string> types;
        for(const auto& entry:runtimeComponentRegistry().metadata().types())
            if(entry.first!="azure.transform" && entry.first!="azure.renderable") types.push_back(entry.first);
        ImGui::BeginDisabled(session_->playing() || session_->building());
        if(ImGui::BeginCombo("Add Component","Choose type")){for(const auto& type:types)if(ImGui::Selectable(type.c_str()))try{session_->edit("component.add",{{"type",type}});}catch(const std::exception& error){session_->log(std::string("ERROR: ")+error.what());}ImGui::EndCombo();}
        const auto& componentRegistry=runtimeComponentRegistry().metadata();
        for(const auto& type:types){auto data=view.componentData(view.selectedNode()->id,type);if(data.is_null())continue;
            ImGui::PushID(type.c_str());
            const bool open=ImGui::CollapsingHeader(type.c_str());observeWidget("component."+type);
            if(ImGui::BeginPopupContextItem("Component actions")){
                if(ImGui::MenuItem("Remove component"))session_->edit("component.remove",{{"type",type}});
                observeWidget("component.remove."+type);ImGui::EndPopup();
            }
            if(open)for(const auto& field:componentRegistry.type(type).properties){
                if(!field.toolVisible)continue;ImGui::PushID(field.name.c_str());
                ImGui::TextUnformatted(field.label.c_str());ImGui::SetNextItemWidth(-1);ImGui::BeginDisabled(field.readOnly);
                auto value=data.at(field.name);bool changed=false;
                if(value.is_boolean()){bool v=value.get<bool>();changed=ImGui::Checkbox("##value",&v);value=v;}
                else if(value.is_number_integer()){int v=value.get<int>();changed=ImGui::DragInt("##value",&v,1,static_cast<int>(field.minimum),static_cast<int>(field.maximum));value=v;}
                else if(value.is_number()){float v=value.get<float>();changed=ImGui::DragFloat("##value",&v,0.01F,static_cast<float>(field.minimum),static_cast<float>(field.maximum));value=v;}
                else if(value.is_array() && value.size()==3){auto v=value.get<std::array<float,3>>();changed=ImGui::DragFloat3("##value",v.data(),0.01F,static_cast<float>(field.minimum),static_cast<float>(field.maximum));value=v;}
                else if(value.is_string()){
                    auto text=value.get<std::string>();
                    if(field.reference=="node"){
                        if(ImGui::BeginCombo("##value",text.c_str())){for(const auto& candidate:view.scene().nodes)if(ImGui::Selectable(candidate.name.c_str(),candidate.id==text)){value=candidate.id;changed=true;}ImGui::EndCombo();}
                    }else if(field.reference=="asset"){
                        if(ImGui::BeginCombo("##value",text.empty()?"Choose asset":text.c_str())){
                            if(ImGui::Selectable("None",text.empty())){value="";changed=true;}
                            for(const auto& [id,record]:view.assets().records())if(assetTypeRegistry().matches(record.path,field.assetTypes)&&ImGui::Selectable(record.virtualPath.c_str(),id==text)){value=id;changed=true;}
                            ImGui::EndCombo();
                        }
                    }else{changed=ui::inputText("##value",text,ImGuiInputTextFlags_EnterReturnsTrue);value=text;}
                }
                const bool ended=ImGui::IsItemDeactivatedAfterEdit();
                observeWidget("field."+type+"."+field.name);ImGui::EndDisabled();
                if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))ImGui::SetTooltip("%s%s",field.tooltip.c_str(),field.readOnly?" (Read-only)":" (Right-click to reset)");
                if(ImGui::BeginPopupContextItem("Field actions")){
                    if(ImGui::MenuItem("Reset field",nullptr,false,!field.readOnly))session_->edit("component.reset-field",{{"type",type},{"field",field.name}});
                    observeWidget("field.reset."+type+"."+field.name);ImGui::EndPopup();
                }
                if(changed)session_->edit("component.field",{{"type",type},{"field",field.name},{"value",value}},"component-"+type+":"+field.name);
                if(ended)session_->edit("history.end-edit");
                for(const auto& error:session_->feedback().report())if(error.at("source")=="component.field"&&error.at("message").get<std::string>().find(field.name)!=std::string::npos)ImGui::TextWrapped("%s",error.at("message").get<std::string>().c_str());
                ImGui::PopID();
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
    int aa=static_cast<int>(settings.antiAliasing);
    if(ImGui::Combo("Anti-aliasing",&aa,"Off\0Edge adaptive\0Supersampling 2x\0"))
        session_->edit("render.settings",{{"values",{{"antiAliasing",aa}}}},"render-aa");
    float exposure = settings.grade.exposureEv;
    if (ImGui::SliderFloat("Exposure EV", &exposure, -8.0F, 8.0F)) {
        session_->edit("render.settings",{{"values",{{"exposure",exposure}}}},"render-exposure");
    }
    ImGui::EndDisabled();
    ImGui::End();
}


}
#endif
