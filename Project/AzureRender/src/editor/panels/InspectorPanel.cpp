#include "editor/ImGuiEditorLayer.hpp"
#include "editor/ui/Widgets.hpp"
#include "editor/ui/UiScopes.hpp"
#include "reflection/Registry.hpp"
#include "runtime/ComponentRegistry.hpp"
#include "runtime/AssetTypeRegistry.hpp"
#include "editor/properties/ObjectReferences.hpp"
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
    if(session_->playing()||session_->building())ui::statusText("当前会话保护文档写入",ui::StatusKind::Warning);
    const auto selected=view.selectedNode()?std::optional<SceneNode>(*view.selectedNode()):std::nullopt;
    if (selected) {
        const auto* node=&*selected;
        ImGui::BeginDisabled(panelContext.selection().selected().size()>1);
        ui::sectionHeading("对象属性");
        ui::statusText(node->id.c_str(),ui::StatusKind::Neutral);
        bool visible = node->visible;
        if (ImGui::Checkbox("可见", &visible)) {
            session_->edit("node.visible",{{"value",visible}});
        }
        observeWidget("inspector.visible");
        ImGui::TextUnformatted("名称");
        auto name=node->name;
        ImGui::SetNextItemWidth(-1);
        if(focusObjectName_){ImGui::SetKeyboardFocusHere();focusObjectName_=false;}
        if(ui::inputText("##name",name))session_->edit("node.rename",{{"value",name}},"details-name");
        if(ImGui::IsItemDeactivatedAfterEdit())session_->edit("history.end-edit");
        observeWidget("name");
        ImGui::EndDisabled();
        drawMaterialSlots(*node);
        if(panelContext.selection().selected().size()==1)drawPhysicsAuthoring(*node);
        ImGui::BeginDisabled(panelContext.selection().selected().size()>1);
        if(view.isProject())for(const auto& link:objectReferences(session_->context(),*node)){
            ImGui::PushID(link.at("label").get<std::string>().c_str());
            ImGui::TextUnformatted(link.at("label").get<std::string>().c_str());
            ui::statusText(link.at("id").get<std::string>().c_str(),link.at("valid").get<bool>()?ui::StatusKind::Neutral:ui::StatusKind::Warning);
            if(ui::button("定位资源",{ui::ButtonVariant::Default,false,!link.at("valid").get<bool>()}))session_->edit("reference.reveal",{{"kind","asset"},{"id",link.at("id")}});
            if(!link.at("message").get<std::string>().empty())ui::resultMessage(link.at("message").get<std::string>(),true);
            ImGui::PopID();
        }
        if(!node->instanceOf.empty()){ImGui::TextUnformatted("预制体内对象标识（只读）");ImGui::TextWrapped("%s",node->instanceOf.c_str());}
        ImGui::EndDisabled();
    }
    const auto owners=panelContext.selection().selected();
    if(owners.empty())ui::emptyState("未选择对象","从层级或场景视口选择对象后编辑属性。");
    else {
        if(owners.size()>1)ui::statusText((std::to_string(owners.size())+" 个对象 · 共享组件").c_str(),ui::StatusKind::Info);
        std::vector<std::string> types{"azure.transform"};
        if(view.isProject()) {
            for(const auto& [type,descriptor]:runtimeComponentRegistry().metadata().types()) {
                (void)descriptor;if(type!="azure.transform"&&type!="azure.renderable")types.push_back(type);
            }
        }
        for(const auto& type:types) {
            const auto fields=PropertyEditorRegistry::selection(view,owners,type);if(fields.empty())continue;
            ImGui::PushID(type.c_str());
            const auto description=runtimeComponentRegistry().describe(type);
            const bool open=ImGui::CollapsingHeader(description.at("label").get<std::string>().c_str(),type=="azure.transform"?ImGuiTreeNodeFlags_DefaultOpen:0);
            observeWidget("component."+type);
            if(ImGui::BeginPopupContextItem("Component actions")) {
                if(description.at("removable").get<bool>()&&ImGui::MenuItem("移除组件",nullptr,false,owners.size()==1))session_->edit("component.remove",{{"type",type}});
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
                        ui::searchField("##Reference search","搜索兼容引用",referenceFilter_);
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
                    if(ui::button("选取")){session_->edit("reference.begin",{{"nodes",owners},{"type",type},{"field",field.name}});}
                    observeWidget("reference.pick."+type+"."+field.name);ui::nextToolbarItem(ui::buttonWidth("定位"));
                    if(ui::button("定位",{ui::ButtonVariant::Default,false,reference.empty()}))session_->edit("reference.reveal",{{"kind",field.reference},{"id",reference}});
                    observeWidget("reference.reveal."+type+"."+field.name);ui::nextToolbarItem(ui::buttonWidth("清空"));
                    if(ui::button("清空",{ui::ButtonVariant::Default,false,field.referenceDefault=="selected-node"})){event.value="";event.changed=true;event.ended=true;}
                    observeWidget("reference.clear."+type+"."+field.name);
                }
                observeWidget("field."+type+"."+field.name);ImGui::EndDisabled();
                if(!state.at("editable").get<bool>())ImGui::TextDisabled("%s",field.readOnly?"只读属性":"此属性仅支持单个对象");
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
                for(const auto& error:session_->feedback().report())if(error.at("source")=="component.batch-field"&&error.at("message").get<std::string>().find(field.name)!=std::string::npos)ui::resultMessage(error.at("message").get<std::string>(),true);
                ImGui::PopID();
            }ImGui::PopID();
        }
        if(view.isProject()){
            ImGui::Separator();ImGui::BeginDisabled(owners.size()!=1);
            if(ui::button("添加组件"))ImGui::OpenPopup("Add component");observeWidget("component.add");
            if(ImGui::BeginPopup("Add component")){
                ui::searchField("##Component search","搜索名称、类型或分类",componentFilter_);
                std::map<std::string,std::vector<nlohmann::json>> groups;
                for(const auto& entry:runtimeComponentRegistry().metadata().types()){
                    const auto info=runtimeComponentRegistry().describe(entry.first);
                    if(!info.at("removable").get<bool>()||!session_->context().componentData(owners.front(),entry.first).is_null())continue;
                    const auto search=info.at("label").get<std::string>()+" "+entry.first+" "+info.at("category").get<std::string>();
                    if(componentFilter_.PassFilter(search.c_str()))groups[info.at("category").get<std::string>()].push_back(info);
                }
                std::string add;
                for(const auto& group:groups){ImGui::TextDisabled("%s",group.first.c_str());for(const auto& info:group.second){
                    bool compatible=true;for(const auto& conflict:info.at("conflicts"))if(!session_->context().componentData(owners.front(),conflict.get<std::string>()).is_null())compatible=false;
                    ImGui::BeginDisabled(!compatible);
                    if(ImGui::Selectable((info.at("label").get<std::string>()+"##"+info.at("type").get<std::string>()).c_str()))add=info.at("type");
                    observeWidget("component.add."+info.at("type").get<std::string>());ImGui::EndDisabled();
                    if(!compatible&&ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))ImGui::SetTooltip("与现有组件冲突；请使用单独的对象。");
                }}
                ImGui::EndPopup();if(!add.empty())session_->edit("component.add",{{"type",add}});
            }
            ImGui::EndDisabled();
        }
        if(view.isProject()&&owners.size()==1){
            ImGui::Separator();
            const bool hasScript=!view.componentData(owners.front(),"azure.script").is_null();
            ImGui::BeginDisabled(hasScript);
            if(ui::button("附加脚本"))ImGui::OpenPopup("Attach script");observeWidget("script.attach-picker");
            if(ImGui::BeginDragDropTarget()){
                if(const auto* payload=ImGui::AcceptDragDropPayload("AZURE_RESOURCE"))session_->edit("script.attach",{{"node",owners.front()},{"asset",static_cast<const char*>(payload->Data)}});
                ImGui::EndDragDropTarget();
            }
            if(ImGui::BeginPopup("Attach script")){
                for(const auto* type:{"script","managed-script"})for(const auto& asset:session_->edit("assets.catalog",{{"type",type}}).value){
                    const auto path=asset.at("path").get<std::string>();
                    const auto extension=std::filesystem::u8path(path).extension();
                    if(extension!=".lua"&&extension!=".azscript")continue;
                    if(ImGui::Selectable(path.c_str()))session_->edit("script.attach",{{"node",owners.front()},{"asset",asset.at("id")}});
                    observeWidget("script.attach."+asset.at("id").get<std::string>());
                }
                if(ImGui::MenuItem("新建脚本…")){workspace_.setVisible("scripts",true);ImGui::SetWindowFocus("Script Authoring###scripts");}
                ImGui::EndPopup();
            }
            ImGui::EndDisabled();
            if(hasScript)ImGui::TextDisabled("在脚本组件中更换脚本引用。");
            else ImGui::TextDisabled("也可将脚本资源拖到附加脚本按钮。");
        }
        if(session_->references().active()) {
            ImGui::Separator();ui::statusText("从层级、视口或资源面板选择兼容引用；Esc 取消。",ui::StatusKind::Info);
            if(ui::button("取消选取"))session_->edit("reference.cancel");observeWidget("reference.cancel");
        }
    }
    ImGui::EndDisabled();
    ImGui::End();
}


}
#endif
