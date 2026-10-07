#include "editor/ImGuiEditorLayer.hpp"
#include "editor/ui/Widgets.hpp"
#include "editor/ui/UiScopes.hpp"
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
    outlinerFilter_.Draw("##Search objects",-1);observeWidget("outliner.search");
    const auto& nodes = view.scene().nodes;
    if (ui::button("Add")) {
        if (!nodes.empty()) {
            try {
                session_->edit("node.child",{{"parent",view.selectedNodeIndex()}});
            } catch (const std::exception& exception) {
                session_->log(std::string("ERROR: ") + exception.what());
            }
        }
    }
    ImGui::SameLine();
    if(ui::button("Actions"))ImGui::OpenPopup("Node actions");
    if(ImGui::BeginPopup("Node actions")) {
        if(ImGui::MenuItem("Delete",nullptr,false,!nodes.empty()))session_->edit("node.delete");
        if(ImGui::MenuItem("Duplicate",nullptr,false,!nodes.empty()))session_->edit("node.duplicate");
        ImGui::EndPopup();
    }
    if (nodes.empty()) {
        ImGui::TextUnformatted("(empty scene)");
        ImGui::End();
        return;
    }
    std::string reveal;
    if(panelContext.selection().consumeReveal()) {
        reveal=panelContext.selection().active();
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
    ImGui::SameLine();ImGui::Selectable("Root",false,0,{40*dpi_,ImGui::GetFrameHeight()});observeWidget("hierarchy.root");
    if(ImGui::IsItemHovered())ImGui::SetTooltip("Drop an object here to move it to the scene root");
    if(ImGui::BeginDragDropTarget()) {
        if(const auto* payload=ImGui::AcceptDragDropPayload("AZURE_NODE"))drop={{"id",static_cast<const char*>(payload->Data)},{"parent",""}};
        ImGui::EndDragDropTarget();
    }
    ImGui::Separator();ImGui::BeginChild("hierarchy-list",{0,0});
    const auto dragItem=[&](const SceneNode& node) {
        if(ImGui::BeginDragDropSource()) {
            ImGui::SetDragDropPayload("AZURE_NODE",node.id.c_str(),node.id.size()+1);
            ImGui::TextUnformatted(node.name.c_str());ImGui::EndDragDropSource();
        }
        if(ImGui::BeginDragDropTarget()) {
            if(const auto* payload=ImGui::AcceptDragDropPayload("AZURE_NODE"))drop={{"id",static_cast<const char*>(payload->Data)},{"parent",node.id}};
            ImGui::EndDragDropTarget();
        }
        if(ImGui::IsItemHovered())ImGui::SetTooltip("%s\n%s\nDouble-click: frame object",node.name.c_str(),node.id.c_str());
    };
    const auto collect=[&](const auto& self,const std::string& parent)->void {
        for(const auto& node:nodes)if(node.parentId==parent){visible.push_back(node.id);if(openNodeIds_.count(node.id))self(self,node.id);}
    };
    if(outlinerFilter_.IsActive()) {
        for(const auto& node:nodes)if(outlinerFilter_.PassFilter(node.name.c_str())||outlinerFilter_.PassFilter(node.id.c_str()))visible.push_back(node.id);
    }else collect(collect,"");
    const auto selectionClick=[&](const SceneNode& node) {
        const auto& io=ImGui::GetIO();
        if(ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !io.KeyCtrl && !io.KeyShift) {
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
    if(outlinerFilter_.IsActive()) {
        for(std::size_t index=0;index<nodes.size();++index)if(std::find(visible.begin(),visible.end(),nodes[index].id)!=visible.end()) {
            ImGui::PushID(nodes[index].id.c_str());ImGui::Selectable(label(nodes[index]).c_str(),selected(index),0,{ImGui::GetContentRegionAvail().x,0});
            observeWidget("node."+nodes[index].id);selectionClick(nodes[index]);dragItem(nodes[index]);
            ImGui::SameLine();ImGui::TextDisabled("%s",nodes[index].resourceId.empty()?"Node":"Mesh");ImGui::PopID();
        }
    }else drawNode(drawNode,"");
    ImGui::EndChild();if(drop)session_->edit("node.reparent",*drop);
    ImGui::End();
}


}
#endif
