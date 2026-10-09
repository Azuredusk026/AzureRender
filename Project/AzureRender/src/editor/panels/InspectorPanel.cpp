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
        ImGui::BeginDisabled(panelContext.selection().selected().size()>1);
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
        ImGui::EndDisabled();
    }
    const auto owners=panelContext.selection().selected();
    if(owners.empty())ImGui::TextWrapped("Select an object in the outliner or viewport to edit its properties.");
    else {
        if(owners.size()>1)ImGui::Text("%zu objects selected | Shared components",owners.size());
        std::vector<std::string> types{"azure.transform"};
        if(view.isProject()) {
            ImGui::BeginDisabled(owners.size()>1);
            if(ImGui::BeginCombo("Add Component","Choose type")) {
                for(const auto& [type,descriptor]:runtimeComponentRegistry().metadata().types()) {
                    (void)descriptor;
                    if(type!="azure.transform"&&type!="azure.renderable"&&ImGui::Selectable(type.c_str()))session_->edit("component.add",{{"type",type}});
                }ImGui::EndCombo();
            }
            ImGui::EndDisabled();
            for(const auto& [type,descriptor]:runtimeComponentRegistry().metadata().types()) {
                (void)descriptor;if(type!="azure.transform"&&type!="azure.renderable")types.push_back(type);
            }
        }
        for(const auto& type:types) {
            const auto fields=PropertyEditorRegistry::selection(view,owners,type);if(fields.empty())continue;
            ImGui::PushID(type.c_str());
            const bool open=ImGui::CollapsingHeader(type=="azure.transform"?"Transform":type.c_str(),type=="azure.transform"?ImGuiTreeNodeFlags_DefaultOpen:0);
            observeWidget("component."+type);
            if(ImGui::BeginPopupContextItem("Component actions")) {
                if(type!="azure.transform"&&ImGui::MenuItem("Remove component",nullptr,false,owners.size()==1))session_->edit("component.remove",{{"type",type}});
                observeWidget("component.remove."+type);ImGui::EndPopup();
            }
            if(open)for(const auto& field:runtimeComponentRegistry().metadata().type(type).properties) {
                if(!field.toolVisible)continue;ImGui::PushID(field.name.c_str());
                const auto& state=fields.at(field.name);ImGui::BeginDisabled(!state.at("editable").get<bool>());
                PropertyEditEvent event;event.value=state.at("value");
                if(field.reference.empty()) {
                    event=ui::propertyEditor(field,state,runtimeComponentRegistry().defaults(type).at("data").at(field.name),dpi_,[&](const auto& id){
                        const auto split=id.find(field.name);observeWidget(id.substr(0,split)+type+"."+id.substr(split));
                    });
                }else {
                    ImGui::TextUnformatted(field.label.c_str());ImGui::SetNextItemWidth(-1);
                    const auto reference=event.value.get<std::string>();
                    if(ImGui::BeginCombo("##value",state.at("mixed").get<bool>()?"Mixed":reference.empty()?"Choose reference":reference.c_str())) {
                        referenceFilter_.Draw("Search",-1);
                        if(field.reference=="node")for(const auto& node:view.scene().nodes) {
                            if((referenceFilter_.PassFilter(node.name.c_str())||referenceFilter_.PassFilter(node.id.c_str()))&&ImGui::Selectable((node.name+"##"+node.id).c_str(),reference==node.id)){event.value=node.id;event.changed=true;}
                        }
                        if(field.reference=="asset")for(const auto& [id,record]:view.assets().records()) {
                            if(assetTypeRegistry().matches(record.path,field.assetTypes)&&referenceFilter_.PassFilter(record.virtualPath.c_str())&&ImGui::Selectable(record.virtualPath.c_str(),reference==id)){event.value=id;event.changed=true;}
                        }ImGui::EndCombo();
                    }
                    observeWidget("field."+type+"."+field.name);
                    if(ImGui::BeginDragDropTarget()) {
                        const auto* payload=ImGui::AcceptDragDropPayload(field.reference=="node"?"AZURE_NODE":"AZURE_RESOURCE");
                        if(payload){event.value=std::string(static_cast<const char*>(payload->Data));event.changed=true;event.ended=true;}ImGui::EndDragDropTarget();
                    }
                    if(ui::button("Pick")){session_->edit("reference.begin",{{"nodes",owners},{"type",type},{"field",field.name}});}
                    observeWidget("reference.pick."+type+"."+field.name);ImGui::SameLine();
                    if(ui::button("Reveal",{ui::ButtonVariant::Default,false,reference.empty()}))session_->edit("reference.reveal",{{"kind",field.reference},{"id",reference}});
                    observeWidget("reference.reveal."+type+"."+field.name);ImGui::SameLine();
                    if(ui::button("Clear",{ui::ButtonVariant::Default,false,field.referenceDefault=="selected-node"})){event.value="";event.changed=true;event.ended=true;}
                    observeWidget("reference.clear."+type+"."+field.name);
                }
                observeWidget("field."+type+"."+field.name);ImGui::EndDisabled();
                if(!state.at("editable").get<bool>())ImGui::TextDisabled("%s",field.readOnly?"Read-only field":"This field supports one object at a time");
                if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)&&!field.tooltip.empty())ImGui::SetTooltip("%s",field.tooltip.c_str());
                if(ImGui::BeginPopupContextItem("Field actions")) {
                    if(ImGui::MenuItem("Reset field",nullptr,false,state.at("editable").get<bool>()))session_->edit("component.batch-field",{{"nodes",owners},{"type",type},{"field",field.name},{"reset",true}});
                    observeWidget("field.reset."+type+"."+field.name);ImGui::EndPopup();
                }
                if(event.changed) {
                    nlohmann::json args={{"nodes",owners},{"type",type},{"field",field.name},{"value",event.value}};
                    if(event.axis)args["axis"]=*event.axis;
                    session_->edit("component.batch-field",args,"property:"+type+":"+field.name);
                }
                if(event.ended||event.cancelled)session_->edit("history.end-edit");
                for(const auto& error:session_->feedback().report())if(error.at("source")=="component.batch-field"&&error.at("message").get<std::string>().find(field.name)!=std::string::npos)ImGui::TextWrapped("%s",error.at("message").get<std::string>().c_str());
                ImGui::PopID();
            }ImGui::PopID();
        }
        if(session_->references().active()) {
            ImGui::Separator();ImGui::TextWrapped("Pick a compatible reference from the outliner, viewport or content browser. Esc cancels.");
            if(ui::button("Cancel picking"))session_->edit("reference.cancel");observeWidget("reference.cancel");
        }
    }
    ImGui::EndDisabled();
    ImGui::End();
}


}
#endif
