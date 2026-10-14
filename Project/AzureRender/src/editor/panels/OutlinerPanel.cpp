#include "editor/ImGuiEditorLayer.hpp"
#include "editor/ui/Widgets.hpp"
#include "editor/ui/UiScopes.hpp"
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
void ImGuiEditorLayer::drawOutlinerPanel(PanelContext& panelContext) {
    const auto& view=panelContext.view();
#ifndef IMGUI_HAS_DOCK
    setFallbackPanelRect(0.0F, 0.0F, 0.20F, 0.72F);
#endif
    if(!ImGui::Begin("Scene Outliner###outliner",workspace_.open("outliner"))){ImGui::End();return;}
    {const auto p=ImGui::GetWindowPos(),s=ImGui::GetWindowSize();widgets_["outliner.drop-region"]={p.x,p.y,s.x,s.y};}
    ui::searchField("##Search objects","搜索对象名称或标识",outlinerFilter_);observeWidget("outliner.search");
    const auto& nodes = view.scene().nodes;
    std::function<void()> pending;
    const bool writable=!session_->playing()&&!session_->building();
    ImGui::BeginDisabled(!writable);
    if(ui::button("创建"))ImGui::OpenPopup("Create root");
    observeWidget("outliner.create");
    if(ImGui::BeginPopup("Create root")){
        const auto parent=view.selectedNode()?view.selectedNode()->id:std::string();
        ui::objectCreationMenu(session_->nodePresets().describe(),[&](const auto& preset){pending=[this,preset,parent]{session_->edit("node.create-preset",{{"preset",preset},{"parent",parent}});};},[&](const auto& id){observeWidget(id);});
        ImGui::EndPopup();
    }
    ImGui::EndDisabled();
    std::string reveal;
    if(panelContext.selection().consumeReveal()) {
        reveal=panelContext.selection().active();
        if(!reveal.empty())openNodeIds_.insert(reveal);
        auto parent=reveal;
        while(!parent.empty()) {
            auto it=std::find_if(nodes.begin(),nodes.end(),[&](const auto& node){return node.id==parent;});
            if(it==nodes.end())break;
            parent=it->parentId;if(!parent.empty())openNodeIds_.insert(parent);
        }
        const auto active=std::find_if(nodes.begin(),nodes.end(),[&](const auto& node){return node.id==reveal;});
        if(active!=nodes.end() && outlinerFilter_.IsActive() && !outlinerFilter_.PassFilter(active->name.c_str()) && !outlinerFilter_.PassFilter(active->id.c_str()))outlinerFilter_.Clear();
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
    const auto sceneLabel=view.scene().sceneId+(session_->context().dirty()?" *":"")+"###scene-container";
    const bool sceneOpen=ImGui::TreeNodeEx(sceneLabel.c_str(),ImGuiTreeNodeFlags_DefaultOpen|ImGuiTreeNodeFlags_SpanAvailWidth|ImGuiTreeNodeFlags_OpenOnArrow);
    observeWidget("hierarchy.root");acceptDrop("");
    if(ImGui::IsItemHovered())ImGui::SetTooltip("场景容器：拖入对象可移到场景顶层");
    const auto dragItem=[&](const SceneNode& node) {
        if(ImGui::BeginPopupContextItem("Object actions")){
            const auto id=node.id;
            const auto select=[this,id]{session_->selection().set({id});};
            if(ImGui::MenuItem("重命名","Details"))pending=[this,select]{select();workspace_.setVisible("inspector",true);focusObjectName_=true;ImGui::SetWindowFocus("Details###inspector");};
            if(ImGui::MenuItem("聚焦对象","F"))pending=[this,select]{select();session_->edit("viewport.frame-selection");};
            const bool writable=!session_->playing()&&!session_->building();
            if(ImGui::MenuItem("复制","Ctrl+D",false,writable))pending=[this,select]{select();session_->edit("node.duplicate");};
            observeWidget("outliner.duplicate");
            if(ImGui::MenuItem("删除","Delete",false,writable))pending=[this,select]{select();session_->edit("node.delete");};
            observeWidget("outliner.delete");
            if(ImGui::BeginMenu("创建子对象",writable)){
                ui::objectCreationMenu(session_->nodePresets().describe(),[&](const auto& preset){pending=[this,preset,id]{session_->edit("node.create-preset",{{"preset",preset},{"parent",id}});};});ImGui::EndMenu();
            }
            if(ImGui::BeginMenu("创建根对象",writable)){
                ui::objectCreationMenu(session_->nodePresets().describe(),[&](const auto& preset){pending=[this,preset]{session_->edit("node.create-preset",{{"preset",preset}});};});ImGui::EndMenu();
            }
            if(ImGui::MenuItem("导出子树",nullptr,false,writable&&view.isProject()&&!session_->context().dirty()))pending=[this,id]{exportRequest_={{"node",id}};exportDialogRequested_=true;};
            observeWidget("outliner.export");
            ImGui::EndPopup();
        }
        if(ImGui::BeginDragDropSource()) {
            ImGui::SetDragDropPayload("AZURE_NODE",node.id.c_str(),node.id.size()+1);
            ImGui::TextUnformatted(node.name.c_str());ImGui::EndDragDropSource();
        }
        // The top edge inserts a sibling; the row body parents the drop.
        const bool sibling=ImGui::GetIO().MousePos.y<ImGui::GetItemRectMin().y+5*dpi_;
        if(sibling && ImGui::GetDragDropPayload()) {
            const auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();
            ImGui::GetWindowDrawList()->AddLine(a,{b.x,a.y},ImGui::GetColorU32(ImGuiCol_DragDropTarget),2*dpi_);
        }
        acceptDrop(sibling?node.parentId:node.id,sibling?node.id:std::string());
        if(ImGui::IsItemHovered())ImGui::SetTooltip("%s\n%s\nDouble-click: frame object",node.name.c_str(),node.id.c_str());
    };
    const auto collect=[&](const auto& self,const std::string& parent)->void {
        for(const auto& node:nodes)if(node.parentId==parent){visible.push_back(node.id);if(openNodeIds_.count(node.id))self(self,node.id);}
    };
    if(outlinerFilter_.IsActive()) {
        for(const auto& node:nodes)if(outlinerFilter_.PassFilter(node.name.c_str())||outlinerFilter_.PassFilter(node.id.c_str()))visible.push_back(node.id);
    }else collect(collect,"");
    if(visible.empty()){
        ui::emptyState("没有匹配的对象","清除搜索后查看场景层级。");
        if(ui::button("清除搜索"))outlinerFilter_.Clear();observeWidget("outliner.clear-search");
    }
    std::string referenceCandidate;
    const auto selectionClick=[&](const SceneNode& node) {
        const auto& io=ImGui::GetIO();
        if(session_->references().active()&&ImGui::IsItemClicked()) {
            referenceCandidate=node.id;
        }else if(ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !io.KeyCtrl && !io.KeyShift) {
            panelContext.selection().set({node.id});session_->edit("viewport.frame-selection");
        }else if(ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
            session_->edit("selection.click",{{"id",node.id},{"ctrl",io.KeyCtrl},{"shift",io.KeyShift},{"visible",visible}});
        }
        if(node.id==reveal)ImGui::SetScrollHereY(.5F);
    };
    const auto label=[&](const SceneNode& node){
        auto text=node.name;const auto width=std::max(24.F,ImGui::GetWindowWidth()-ImGui::GetCursorPosX()-48*dpi_);
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
                ImGui::SetNextItemOpen(openNodeIds_.count(node.id)>0,ImGuiCond_Always);
                open=ImGui::TreeNodeEx(label(node).c_str(),ImGuiTreeNodeFlags_OpenOnArrow|ImGuiTreeNodeFlags_SpanAvailWidth|(selected(index)?ImGuiTreeNodeFlags_Selected:0));
                if(open)openNodeIds_.insert(node.id);else openNodeIds_.erase(node.id);
            }else ImGui::Selectable(label(node).c_str(),selected(index),0,{ImGui::GetContentRegionAvail().x,0});
            observeWidget("node."+node.id);selectionClick(node);dragItem(node);
            if(open){self(self,node.id);ImGui::TreePop();}ImGui::PopID();
        }
    };
    if(sceneOpen) {
    if(outlinerFilter_.IsActive()) {
        for(std::size_t index=0;index<nodes.size();++index)if(std::find(visible.begin(),visible.end(),nodes[index].id)!=visible.end()) {
            ImGui::PushID(nodes[index].id.c_str());ImGui::Selectable(label(nodes[index]).c_str(),selected(index),0,{ImGui::GetContentRegionAvail().x,0});
            observeWidget("node."+nodes[index].id);selectionClick(nodes[index]);dragItem(nodes[index]);
            ImGui::SameLine();ImGui::TextDisabled("%s",nodes[index].resourceId.empty()?"Node":"Mesh");ImGui::PopID();
        }
    }else drawNode(drawNode,"");
    ImGui::TreePop();
    }
    ImGui::InvisibleButton("##root-drop-space",{ImGui::GetContentRegionAvail().x,std::max(24.F,ImGui::GetContentRegionAvail().y)});
    observeWidget("hierarchy.empty");acceptDrop("");
    if(ImGui::IsItemClicked()&&!ImGui::GetDragDropPayload())panelContext.selection().set({});
    if(ImGui::BeginPopupContextItem("Hierarchy creation")){
        ImGui::BeginDisabled(!writable);
        ui::objectCreationMenu(session_->nodePresets().describe(),[&](const auto& preset){pending=[this,preset]{session_->edit("node.create-preset",{{"preset",preset}});};});ImGui::EndDisabled();ImGui::EndPopup();
    }
    ImGui::EndChild();
    if(!referenceCandidate.empty())session_->edit("reference.deliver",{{"kind","node"},{"id",referenceCandidate}});
    ImGui::End();
    if(drop)session_->edit("node.move",*drop);
    if(pending)pending();
}


}
#endif
