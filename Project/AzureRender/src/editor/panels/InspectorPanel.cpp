#include "ui/localization/Localization.hpp"
#include "editor/properties/PropertyWidgets.hpp"
#include "CorePanels.hpp"
#include "editor/PanelContext.hpp"
#include "ui/Widgets.hpp"
#include "ui/UiScopes.hpp"
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
void InspectorPanel::draw(PanelContext& panelContext) {
    const auto& view=panelContext.view();
#ifndef IMGUI_HAS_DOCK
    setFallbackPanelRect(0.76F, 0.0F, 0.24F, 0.72F);
#endif
    if(!ImGui::Begin(ui::tr("属性###inspector"),services_.workspace.open("inspector"))){ImGui::End();return;}
    auto& filter=presentation.propertyFilter_;
    const auto* clearSearch=ui::tr("清除搜索");
    const auto clearWidth=ui::buttonWidth(clearSearch);
    const auto searchWidth=filter.IsActive()
        ?std::max(1.F,ImGui::GetContentRegionAvail().x-clearWidth-ImGui::GetStyle().ItemSpacing.x)
        :-1.F;
    ui::searchField("##Property filter",ui::tr("搜索组件或属性"),filter,searchWidth);
    services_.observe("inspector.search");
    if(filter.IsActive()){
        ui::nextToolbarItem(clearWidth);
        if(ui::button(clearSearch))filter.Clear();
        services_.observe("inspector.clear-search");
    }
    const bool searching=filter.IsActive();
    ImGui::BeginDisabled(services_.session.playing() || services_.session.building());
    if(services_.session.playing()||services_.session.building())ui::statusText(ui::tr("当前会话保护文档写入"),ui::StatusKind::Warning);
    const auto selected=view.selectedNode()?std::optional<SceneNode>(*view.selectedNode()):std::nullopt;
    if (selected) {
        const auto* node=&*selected;
        ImGui::BeginDisabled(panelContext.selection().selected().size()>1);
        ui::sectionHeading(ui::tr("对象属性"));
        ui::statusText(node->id.c_str(),ui::StatusKind::Neutral);
        bool visible = node->visible;
        if (ImGui::Checkbox(ui::tr("可见"), &visible)) {
            services_.session.edit("node.visible",{{"value",visible}});
        }
        services_.observe("inspector.visible");
        ImGui::TextUnformatted(ui::tr("名称"));
        auto name=node->name;
        ImGui::SetNextItemWidth(-1);
        if(this->presentation.focusObjectName_){ImGui::SetKeyboardFocusHere();this->presentation.focusObjectName_=false;}
        if(ui::inputText("##name",name))services_.session.edit("node.rename",{{"value",name}},"details-name");
        if(ImGui::IsItemDeactivatedAfterEdit())services_.session.edit("history.end-edit");
        services_.observe("name");
        ImGui::EndDisabled();
        if(!view.componentData(node->id,"azure.camera").is_null()){
            if(ui::button(ui::tr("查看相机视角")))services_.session.edit("viewport.camera-preview",{{"node",node->id}});
            services_.observe("camera.preview");
        }
        services_.materialSlots(*node);
        if(panelContext.selection().selected().size()==1)services_.physics(*node);
        ImGui::BeginDisabled(panelContext.selection().selected().size()>1);
        if(view.isProject())for(const auto& link:objectReferences(services_.session.context(),*node)){
            ImGui::PushID(link.at("label").get<std::string>().c_str());
            ImGui::TextUnformatted(ui::tr(link.at("label").get<std::string>().c_str()));
            const auto& identity=link.at("id").get_ref<const std::string&>();
            const auto& path=link.at("path").get_ref<const std::string&>();
            const auto name=path.empty()?identity:std::filesystem::u8path(path).filename().u8string();
            ui::statusText(name.c_str(),link.at("valid").get<bool>()?ui::StatusKind::Neutral:ui::StatusKind::Warning);
            if(ImGui::IsItemHovered())ImGui::SetTooltip("%s",path.empty()?identity.c_str():path.c_str());
            if(ui::button(ui::tr("定位资源"),{ui::ButtonVariant::Default,false,!link.at("valid").get<bool>()}))services_.session.edit("reference.reveal",{{"kind","asset"},{"id",identity}});
            services_.observe("reference.object-asset."+identity);
            ui::nextToolbarItem(ui::buttonWidth(ui::tr("复制标识")));
            if(ui::button(ui::tr("复制标识")))ImGui::SetClipboardText(identity.c_str());
            if(!link.at("message").get<std::string>().empty())ui::resultMessage(link.at("message").get<std::string>(),true);
            ImGui::PopID();
        }
        if(!node->instanceOf.empty()){ImGui::TextUnformatted(ui::tr("预制体内对象标识（只读）"));ImGui::TextWrapped("%s",node->instanceOf.c_str());}
        ImGui::EndDisabled();
    }
    const auto owners=panelContext.selection().selected();
    if(owners.empty())ui::emptyState(ui::tr("未选择对象"),ui::tr("从层级或场景视口选择对象后编辑属性。"));
    else {
        if(owners.size()>1)ui::statusText((std::to_string(owners.size())+ui::tr(" 个对象 · 共享组件")).c_str(),ui::StatusKind::Info);
        std::vector<std::string> types{"azure.transform"};
        if(view.isProject()) {
            for(const auto& [type,descriptor]:runtimeComponentRegistry().metadata().types()) {
                (void)descriptor;if(type!="azure.transform"&&type!="azure.renderable")types.push_back(type);
            }
        }
        bool hasMatches=false;
        for(const auto& type:types) {
            const auto fields=PropertyEditorRegistry::selection(view,owners,type);if(fields.empty())continue;
            const auto description=runtimeComponentRegistry().describe(type);
            const auto& label=description.at("label").get_ref<const std::string&>();
            const auto* translated=ui::tr(label.c_str());
            std::vector<const reflection::Property*> visibleFields;
            const auto componentTerms=type+" "+label+" "+translated;
            for(const auto& field:runtimeComponentRegistry().metadata().type(type).properties){
                if(!field.toolVisible)continue;
                if(searching){
                    const auto terms=componentTerms+" "+field.name+" "+field.label+" "+ui::tr(field.label.c_str());
                    if(!filter.PassFilter(terms.c_str()))continue;
                }
                visibleFields.push_back(&field);
            }
            if(visibleFields.empty())continue;
            hasMatches=true;
            ImGui::PushID(type.c_str());
            const auto headerFlags=searching?ImGuiTreeNodeFlags_Leaf:
                type=="azure.transform"?ImGuiTreeNodeFlags_DefaultOpen:ImGuiTreeNodeFlags_None;
            const bool open=ImGui::CollapsingHeader(translated,headerFlags);
            services_.observe("component."+type);
            if(ImGui::BeginPopupContextItem(ui::tr("Component actions"))) {
                if(description.at("removable").get<bool>()&&ImGui::MenuItem(ui::tr("移除组件"),nullptr,false,owners.size()==1))services_.session.edit("component.remove",{{"type",type}});
                services_.observe("component.remove."+type);ImGui::EndPopup();
            }
            if(open)for(const auto* property:visibleFields) {
                const auto& field=*property;
                ImGui::PushID(field.name.c_str());
                const auto& state=fields.at(field.name);ImGui::BeginDisabled(!state.at("editable").get<bool>());
                PropertyEditEvent event;event.value=state.at("value");
                if(field.reference.empty()) {
                    event=ui::propertyEditor(field,state,runtimeComponentRegistry().defaults(type).at("data").at(field.name),services_.dpi,[&](const auto& id){
                        const auto split=id.find(field.name);services_.observe(id.substr(0,split)+type+"."+id.substr(split));
                    });
                }else {
                    ImGui::TextUnformatted(ui::tr(field.label.c_str()));ImGui::SetNextItemWidth(-1);
                    const auto reference=event.value.get<std::string>();
                    std::string caption;
                    if(field.reference=="node"){
                        const auto& nodes=view.scene().nodes;
                        const auto item=std::find_if(nodes.begin(),nodes.end(),[&](const auto& node){return node.id==reference;});
                        if(item!=nodes.end())caption=item->name;
                    }else if(view.isProject()){
                        const auto& records=view.assets().records();
                        const auto item=records.find(reference);
                        if(item!=records.end())caption=item->second.virtualPath;
                        else if(reference.find(":/")!=std::string::npos)caption=reference;
                    }
                    if(caption.empty())caption=ui::tr(reference.empty()?"Choose reference":"缺失引用");
                    const bool mixed=state.at("mixed").get<bool>();
                    if(ImGui::BeginCombo("##value",mixed?ui::tr("Mixed"):caption.c_str())) {
                        ui::searchField("##Reference search",ui::tr("搜索兼容引用"),this->presentation.referenceFilter_);
                        if(field.reference=="node")for(const auto& node:view.scene().nodes) {
                            if((this->presentation.referenceFilter_.PassFilter(node.name.c_str())||this->presentation.referenceFilter_.PassFilter(node.id.c_str()))&&ImGui::Selectable((node.name+"##"+node.id).c_str(),reference==node.id)){event.value=node.id;event.changed=true;}
                        }
                        if(field.reference=="asset")for(const auto& [id,record]:view.assets().records()) {
                            if(assetTypeRegistry().matches(record.path,field.assetTypes)&&this->presentation.referenceFilter_.PassFilter(record.virtualPath.c_str())&&ImGui::Selectable(record.virtualPath.c_str(),reference==id)){event.value=id;event.changed=true;}
                        }ImGui::EndCombo();
                    }
                    services_.observe("field."+type+"."+field.name);
                    if(!mixed&&!reference.empty()&&ImGui::IsItemHovered())ImGui::SetTooltip("%s",reference.c_str());
                    if(ImGui::BeginDragDropTarget()) {
                        const auto* payload=ImGui::AcceptDragDropPayload(field.reference=="node"?"AZURE_NODE":"AZURE_RESOURCE");
                        if(payload){event.value=std::string(static_cast<const char*>(payload->Data));event.changed=true;event.ended=true;}ImGui::EndDragDropTarget();
                    }
                    if(ui::button(ui::tr("选取"))){services_.session.edit("reference.begin",{{"nodes",owners},{"type",type},{"field",field.name}});}
                    services_.observe("reference.pick."+type+"."+field.name);ui::nextToolbarItem(ui::buttonWidth(ui::tr("定位")));
                    if(ui::button(ui::tr("定位"),{ui::ButtonVariant::Default,false,mixed||reference.empty()}))services_.session.edit("reference.reveal",{{"kind",field.reference},{"id",reference}});
                    services_.observe("reference.reveal."+type+"."+field.name);ui::nextToolbarItem(ui::buttonWidth(ui::tr("清空")));
                    if(ui::button(ui::tr("清空"),{ui::ButtonVariant::Default,false,field.referenceDefault=="selected-node"})){event.value="";event.changed=true;event.ended=true;}
                    services_.observe("reference.clear."+type+"."+field.name);
                    if(!mixed&&!reference.empty()){
                        ui::nextToolbarItem(ui::buttonWidth(ui::tr("复制标识")));
                        if(ui::button(ui::tr("复制标识")))ImGui::SetClipboardText(reference.c_str());
                        services_.observe("reference.copy."+type+"."+field.name);
                    }
                }
                services_.observe("field."+type+"."+field.name);ImGui::EndDisabled();
                if(!state.at("editable").get<bool>())ImGui::TextDisabled("%s",field.readOnly?ui::tr("只读属性"):ui::tr("此属性仅支持单个对象"));
                if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)&&!field.tooltip.empty())ImGui::SetTooltip("%s",field.tooltip.c_str());
                if(ImGui::BeginPopupContextItem(ui::tr("Field actions"))) {
                    if(ImGui::MenuItem(ui::tr("Reset field"),nullptr,false,state.at("editable").get<bool>()))services_.session.edit("component.batch-field",{{"nodes",owners},{"type",type},{"field",field.name},{"reset",true}});
                    services_.observe("field.reset."+type+"."+field.name);ImGui::EndPopup();
                }
                if(event.cancelled)services_.session.edit("history.cancel-edit");
                else if(event.changed) {
                    nlohmann::json args={{"nodes",owners},{"type",type},{"field",field.name},{"value",event.value}};
                    if(event.axis)args["axis"]=*event.axis;
                    services_.session.edit("component.batch-field",args,"property:"+type+":"+field.name);
                }
                if(event.ended&&!event.cancelled)services_.session.edit("history.end-edit");
                for(const auto& error:services_.session.feedback().report())if(error.at("source")=="component.batch-field"&&error.at("message").get<std::string>().find(field.name)!=std::string::npos)ui::resultMessage(error.at("message").get<std::string>(),true);
                ImGui::PopID();
            }ImGui::PopID();
        }
        if(searching&&!hasMatches){
            ui::statusText(ui::tr("没有匹配的组件或属性"),ui::StatusKind::Neutral);
            services_.observe("inspector.no-matches");
        }
        if(view.isProject()){
            ImGui::Separator();ImGui::BeginDisabled(owners.size()!=1);
            if(ui::button(ui::tr("添加组件")))ImGui::OpenPopup(ui::tr("Add component"));services_.observe("component.add");
            if(ImGui::BeginPopup(ui::tr("Add component"))){
                auto& componentFilter=presentation.componentFilter_;
                const bool opening=ImGui::IsWindowAppearing();
                if(opening)componentFilter.Clear();
                std::string add;
                if(ImGui::IsKeyPressed(ImGuiKey_Escape,false)){
                    ImGui::CloseCurrentPopup();
                }else{
                    if(opening||presentation.focusComponentSearch_)ImGui::SetKeyboardFocusHere();
                    presentation.focusComponentSearch_=false;
                    const auto* clearLabel=ui::tr("清除搜索");
                    const float clearWidth=ui::buttonWidth(clearLabel)+ImGui::GetStyle().ItemSpacing.x;
                    const float rowWidth=280.F*services_.dpi;
                    const bool showClear=componentFilter.IsActive();
                    const auto filterWidth=showClear?rowWidth-clearWidth:rowWidth;
                    ui::searchField("##Component search",ui::tr("搜索名称、类型或分类"),componentFilter,filterWidth);
                    services_.observe("component.add.search");
                    if(showClear&&componentFilter.IsActive()){
                        ImGui::SameLine();
                        if(ui::button(clearLabel)){
                            componentFilter.Clear();
                            presentation.focusComponentSearch_=true;
                        }
                        services_.observe("component.add.clear-search");
                    }
                    std::map<std::string,std::vector<nlohmann::json>> groups;
                    for(const auto& [type,descriptor]:runtimeComponentRegistry().metadata().types()){
                        (void)descriptor;
                        const auto info=runtimeComponentRegistry().describe(type);
                        if(!info.at("removable").get<bool>()||!view.componentData(owners.front(),type).is_null())continue;
                        const auto& label=info.at("label").get_ref<const std::string&>();
                        const auto& category=info.at("category").get_ref<const std::string&>();
                        const auto terms=label+" "+ui::tr(label.c_str())+" "+type+" "+category+" "+ui::tr(category.c_str());
                        if(componentFilter.PassFilter(terms.c_str()))groups[category].push_back(info);
                    }
                    if(groups.empty()){
                        ui::statusText(ui::tr(componentFilter.IsActive()?"没有匹配的组件":"没有可添加的组件"),ui::StatusKind::Neutral);
                        services_.observe("component.add.no-matches");
                    }
                    for(const auto& [category,options]:groups){
                        ImGui::TextDisabled("%s",ui::tr(category.c_str()));
                        for(const auto& info:options){
                            bool compatible=true;
                            for(const auto& conflict:info.at("conflicts"))
                                if(!view.componentData(owners.front(),conflict.get<std::string>()).is_null())compatible=false;
                            const auto& type=info.at("type").get_ref<const std::string&>();
                            const auto& label=info.at("label").get_ref<const std::string&>();
                            ImGui::BeginDisabled(!compatible);
                            if(ImGui::Selectable((std::string(ui::tr(label.c_str()))+"##"+type).c_str()))add=type;
                            services_.observe("component.add."+type);
                            ImGui::EndDisabled();
                            if(!compatible&&ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                                ImGui::SetTooltip(ui::tr("与现有组件冲突；请使用单独的对象。"));
                        }
                    }
                }
                ImGui::EndPopup();
                if(!add.empty())services_.session.edit("component.add",{{"type",add}});
            }
            ImGui::EndDisabled();
        }
        if(view.isProject()&&owners.size()==1){
            ImGui::Separator();
            const bool hasScript=!view.componentData(owners.front(),"azure.script").is_null();
            ImGui::BeginDisabled(hasScript);
            if(ui::button(ui::tr("附加脚本")))ImGui::OpenPopup(ui::tr("Attach script"));services_.observe("script.attach-picker");
            if(ImGui::BeginDragDropTarget()){
                if(const auto* payload=ImGui::AcceptDragDropPayload("AZURE_RESOURCE"))services_.session.edit("script.attach",{{"node",owners.front()},{"asset",static_cast<const char*>(payload->Data)}});
                ImGui::EndDragDropTarget();
            }
            if(ImGui::BeginPopup(ui::tr("Attach script"))){
                for(const auto* type:{"script","managed-script"})for(const auto& asset:services_.session.edit("assets.catalog",{{"type",type}}).value){
                    const auto path=asset.at("path").get<std::string>();
                    const auto extension=std::filesystem::u8path(path).extension();
                    if(extension!=".lua"&&extension!=".azscript")continue;
                    if(ImGui::Selectable(path.c_str()))services_.session.edit("script.attach",{{"node",owners.front()},{"asset",asset.at("id")}});
                    services_.observe("script.attach."+asset.at("id").get<std::string>());
                }
                if(ImGui::MenuItem(ui::tr("新建脚本…"))){services_.workspace.setVisible("scripts",true);ImGui::SetWindowFocus(ui::tr("脚本制作###scripts"));}
                ImGui::EndPopup();
            }
            ImGui::EndDisabled();
            if(hasScript)ImGui::TextDisabled(ui::tr("在脚本组件中更换脚本引用。"));
            else ImGui::TextDisabled(ui::tr("也可将脚本资源拖到附加脚本按钮。"));
        }
        if(services_.session.references().active()) {
            ImGui::Separator();ui::statusText(ui::tr("从层级、视口或资源面板选择兼容引用；Esc 取消。"),ui::StatusKind::Info);
            if(ui::button(ui::tr("取消选取")))services_.session.edit("reference.cancel");services_.observe("reference.cancel");
        }
    }
    ImGui::EndDisabled();
    ImGui::End();
}


}
#endif
