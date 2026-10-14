#include "CorePanels.hpp"
#include "editor/PanelContext.hpp"
#include "ui/Widgets.hpp"
#include "ui/UiScopes.hpp"
#include "editor/ui/ObjectCreationMenu.hpp"
#include "reflection/Registry.hpp"
#include "runtime/ComponentRegistry.hpp"
#include "diagnostics/RuntimeDiagnostics.hpp"
#ifdef AZURERENDER_HAS_IMGUI
#include <imgui.h>
#include <imgui_internal.h>
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
void OutlinerPanel::draw(PanelContext& panelContext) {
    const auto& view=panelContext.view();
#ifndef IMGUI_HAS_DOCK
    setFallbackPanelRect(0.0F, 0.0F, 0.20F, 0.72F);
#endif
    if(!ImGui::Begin("场景层级###outliner",services_.workspace.open("outliner"))){ImGui::End();return;}
    {const auto p=ImGui::GetWindowPos(),s=ImGui::GetWindowSize();services_.widgets["outliner.drop-region"]={p.x,p.y,s.x,s.y};}
    ui::searchField("##Search objects","搜索对象名称或标识",presentation.outlinerFilter_);services_.observe("outliner.search");
    const auto& nodes = view.scene().nodes;
    std::function<void()> pending;
    const bool writable=!services_.session.playing()&&!services_.session.building();
    ImGui::BeginDisabled(!writable);
    if(ui::button("创建"))ImGui::OpenPopup("Create root");
    services_.observe("outliner.create");
    if(ImGui::BeginPopup("Create root")){
        const auto parent=view.selectedNode()?view.selectedNode()->id:std::string();
        ui::objectCreationMenu(services_.session.nodePresets().describe(),[&](const auto& preset){pending=[this,preset,parent]{services_.session.edit("node.create-preset",{{"preset",preset},{"parent",parent}});};},[&](const auto& id){services_.observe(id);});
        ImGui::EndPopup();
    }
    ImGui::EndDisabled();
    std::string reveal;
    if(panelContext.selection().consumeReveal()) {
        reveal=panelContext.selection().active();
        if(!reveal.empty())presentation.openNodeIds_.insert(reveal);
        auto parent=reveal;
        while(!parent.empty()) {
            auto it=std::find_if(nodes.begin(),nodes.end(),[&](const auto& node){return node.id==parent;});
            if(it==nodes.end())break;
            parent=it->parentId;if(!parent.empty())presentation.openNodeIds_.insert(parent);
        }
        const auto active=std::find_if(nodes.begin(),nodes.end(),[&](const auto& node){return node.id==reveal;});
        if(active!=nodes.end() && presentation.outlinerFilter_.IsActive() && !presentation.outlinerFilter_.PassFilter(active->name.c_str()) && !presentation.outlinerFilter_.PassFilter(active->id.c_str()))presentation.outlinerFilter_.Clear();
    }
    std::vector<std::string> visible;
    std::optional<nlohmann::json> drop;
    const auto acceptDrop=[&](const std::string& parent,const std::string& before=std::string()) {
        if(!writable)return;
        if(ImGui::BeginDragDropTarget()) {
            if(const auto* payload=ImGui::AcceptDragDropPayload("AZURE_NODE")) {
                const std::string id=static_cast<const char*>(payload->Data);
                std::vector<std::string> ids;
                for(auto index:view.selectedNodes())ids.push_back(nodes.at(index).id);
                if(std::find(ids.begin(),ids.end(),id)==ids.end())ids={id};
                drop={{"ids",ids},{"parent",parent},{"before",before}};
            }
            ImGui::EndDragDropTarget();
        }
    };
    ImGui::Separator();ImGui::BeginChild("hierarchy-list",{0,0});
    const auto sceneLabel=view.scene().sceneId+(services_.session.context().dirty()?" *":"")+"###scene-container";
    const bool sceneOpen=ImGui::TreeNodeEx(sceneLabel.c_str(),ImGuiTreeNodeFlags_DefaultOpen|ImGuiTreeNodeFlags_SpanAvailWidth|ImGuiTreeNodeFlags_OpenOnArrow);
    services_.observe("hierarchy.root");acceptDrop("");
    if(ImGui::IsItemClicked()&&!ImGui::IsItemToggledOpen()&&!ImGui::GetDragDropPayload())panelContext.selection().set({});
    if(ImGui::IsItemHovered())ImGui::SetTooltip("场景容器：拖入对象可移到场景顶层");
    const auto dragItem=[&](const SceneNode& node) {
        if(ImGui::BeginPopupContextItem("Object actions")){
            const auto id=node.id;
            const auto select=[this,id]{services_.session.selection().set({id});};
            if(ImGui::MenuItem("重命名","Details"))pending=[this,select]{select();services_.workspace.setVisible("inspector",true);services_.rename();ImGui::SetWindowFocus("属性###inspector");};
            if(ImGui::MenuItem("聚焦对象","F"))pending=[this,select]{select();services_.session.edit("viewport.frame-selection");};
            const bool writable=!services_.session.playing()&&!services_.session.building();
            if(ImGui::MenuItem("复制","Ctrl+D",false,writable))pending=[this,select]{select();services_.session.edit("node.duplicate");};
            services_.observe("outliner.duplicate");
            if(ImGui::MenuItem("删除","Delete",false,writable))pending=[this,select]{select();services_.session.edit("node.delete");};
            services_.observe("outliner.delete");
            if(ImGui::BeginMenu("创建子对象",writable)){
                ui::objectCreationMenu(services_.session.nodePresets().describe(),[&](const auto& preset){pending=[this,preset,id]{services_.session.edit("node.create-preset",{{"preset",preset},{"parent",id}});};});ImGui::EndMenu();
            }
            if(ImGui::BeginMenu("创建根对象",writable)){
                ui::objectCreationMenu(services_.session.nodePresets().describe(),[&](const auto& preset){pending=[this,preset]{services_.session.edit("node.create-preset",{{"preset",preset}});};});ImGui::EndMenu();
            }
            if(ImGui::MenuItem("导出子树",nullptr,false,writable&&view.isProject()&&!services_.session.context().dirty()))pending=[this,id]{services_.exportSelection({{"node",id}});};
            services_.observe("outliner.export");
            ImGui::EndPopup();
        }
        if(ImGui::BeginDragDropSource()) {
            ImGui::SetDragDropPayload("AZURE_NODE",node.id.c_str(),node.id.size()+1);
            ImGui::TextUnformatted(node.name.c_str());ImGui::EndDragDropSource();
        }
        // The top edge inserts a sibling; the row body parents the drop.
        const bool sibling=ImGui::GetIO().MousePos.y<ImGui::GetItemRectMin().y+5*services_.dpi;
        if(sibling && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) && ImGui::GetDragDropPayload()) {
            const auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();
            ImGui::GetWindowDrawList()->AddLine(a,{b.x,a.y},ImGui::GetColorU32(ImGuiCol_DragDropTarget),2*services_.dpi);
        }
        acceptDrop(sibling?node.parentId:node.id,sibling?node.id:std::string());
        if(ImGui::IsItemHovered())ImGui::SetTooltip("%s\n%s\nDouble-click: frame object",node.name.c_str(),node.id.c_str());
    };
    const auto collect=[&](const auto& self,const std::string& parent)->void {
        for(const auto& node:nodes)if(node.parentId==parent){visible.push_back(node.id);if(presentation.openNodeIds_.count(node.id))self(self,node.id);}
    };
    if(presentation.outlinerFilter_.IsActive()) {
        for(const auto& node:nodes)if(presentation.outlinerFilter_.PassFilter(node.name.c_str())||presentation.outlinerFilter_.PassFilter(node.id.c_str()))visible.push_back(node.id);
    }else collect(collect,"");
    if(visible.empty()){
        ui::emptyState("没有匹配的对象","清除搜索后查看场景层级。");
        if(ui::button("清除搜索"))presentation.outlinerFilter_.Clear();services_.observe("outliner.clear-search");
    }
    std::string referenceCandidate;
    const auto selectionClick=[&](const SceneNode& node) {
        const auto& io=ImGui::GetIO();
        if(services_.session.references().active()&&ImGui::IsItemClicked()) {
            referenceCandidate=node.id;
        }else if(ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !io.KeyCtrl && !io.KeyShift) {
            panelContext.selection().set({node.id});services_.session.edit("viewport.frame-selection");
        }else if(ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
            services_.session.edit("selection.click",{{"id",node.id},{"ctrl",io.KeyCtrl},{"shift",io.KeyShift},{"visible",visible}});
        }
        if(node.id==reveal)ImGui::SetScrollHereY(.5F);
    };
    const auto label=[&](const SceneNode& node){
        auto text=node.name;const auto width=std::max(24.F,ImGui::GetWindowWidth()-ImGui::GetCursorPosX()-48*services_.dpi);
        if(ImGui::CalcTextSize(text.c_str()).x<=width)return text;
        while(!text.empty()&&ImGui::CalcTextSize((text+"...").c_str()).x>width){
            auto end=text.size()-1;while(end>0&&(static_cast<unsigned char>(text[end])&0xc0)==0x80)--end;text.resize(end);
        }return text+"...";
    };
    const auto selected=[&](std::size_t index){return std::find(view.selectedNodes().begin(),view.selectedNodes().end(),index)!=view.selectedNodes().end();};
    const auto drawNode=[&](const auto& self,const std::string& parent)->void {
        for(std::size_t index=0;index<nodes.size();++index) {
            const auto& node=nodes[index];if(node.parentId!=parent)continue;
            const bool children=std::any_of(nodes.begin(),nodes.end(),[&](const auto& child){return child.parentId==node.id;});
            ImGui::PushID(node.id.c_str());bool open=false;
            if(children) {
                ImGui::SetNextItemOpen(presentation.openNodeIds_.count(node.id)>0,ImGuiCond_Always);
                open=ImGui::TreeNodeEx(label(node).c_str(),ImGuiTreeNodeFlags_OpenOnArrow|ImGuiTreeNodeFlags_SpanAvailWidth|(selected(index)?ImGuiTreeNodeFlags_Selected:0));
                if(open)presentation.openNodeIds_.insert(node.id);else presentation.openNodeIds_.erase(node.id);
            }else ImGui::Selectable(label(node).c_str(),selected(index),0,{ImGui::GetContentRegionAvail().x,0});
            services_.observe("node."+node.id);selectionClick(node);dragItem(node);
            if(open){self(self,node.id);ImGui::TreePop();}ImGui::PopID();
        }
    };
    if(sceneOpen) {
    if(presentation.outlinerFilter_.IsActive()) {
        for(std::size_t index=0;index<nodes.size();++index)if(std::find(visible.begin(),visible.end(),nodes[index].id)!=visible.end()) {
            ImGui::PushID(nodes[index].id.c_str());ImGui::Selectable(label(nodes[index]).c_str(),selected(index),0,{ImGui::GetContentRegionAvail().x,0});
            services_.observe("node."+nodes[index].id);selectionClick(nodes[index]);dragItem(nodes[index]);
            ImGui::SameLine();ImGui::TextDisabled("%s",nodes[index].resourceId.empty()?"Node":"Mesh");ImGui::PopID();
        }
    }else drawNode(drawNode,"");
    ImGui::TreePop();
    }
    ImGui::InvisibleButton("##root-drop-space",{ImGui::GetContentRegionAvail().x,std::max(24.F,ImGui::GetContentRegionAvail().y)});
    services_.observe("hierarchy.empty");acceptDrop("");
    if(ImGui::IsItemClicked()&&!ImGui::GetDragDropPayload())panelContext.selection().set({});
    if(ImGui::BeginPopupContextItem("Hierarchy creation")){
        ImGui::BeginDisabled(!writable);
        ui::objectCreationMenu(services_.session.nodePresets().describe(),[&](const auto& preset){pending=[this,preset]{services_.session.edit("node.create-preset",{{"preset",preset}});};});ImGui::EndDisabled();ImGui::EndPopup();
    }
    ImGui::EndChild();
    if(!referenceCandidate.empty())services_.session.edit("reference.deliver",{{"kind","node"},{"id",referenceCandidate}});
    ImGui::End();
    if(drop)services_.session.edit("node.move",*drop);
    if(pending)pending();
}


}
#endif
