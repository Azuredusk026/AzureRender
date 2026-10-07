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
    if (ui::button("Add Child")) {
        if (!nodes.empty()) {
            try {
                session_->edit("node.child",{{"parent",view.selectedNodeIndex()}});
            } catch (const std::exception& exception) {
                session_->log(std::string("ERROR: ") + exception.what());
            }
        }
    }
    ImGui::SameLine();
    if (ui::button("Delete") && !nodes.empty()) {
        session_->edit("node.delete");
    }
    ImGui::SameLine();if(ui::button("Duplicate"))session_->edit("node.duplicate");
    ImGui::Separator();
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
    const auto selected=[&](std::size_t index){return std::find(view.selectedNodes().begin(),view.selectedNodes().end(),index)!=view.selectedNodes().end();};
    const auto drawNode=[&](const auto& self,const std::string& parent)->void {
        for(std::size_t index=0;index<nodes.size();++index) {
            const auto& node=nodes[index];if(node.parentId!=parent)continue;
            const bool children=std::any_of(nodes.begin(),nodes.end(),[&](const auto& child){return child.parentId==node.id;});
            ImGui::PushID(node.id.c_str());bool open=false;
            if(children) {
                ImGui::SetNextItemOpen(openNodeIds_.count(node.id)>0,ImGuiCond_Always);
                open=ImGui::TreeNodeEx(node.name.c_str(),ImGuiTreeNodeFlags_OpenOnArrow|ImGuiTreeNodeFlags_SpanAvailWidth|(selected(index)?ImGuiTreeNodeFlags_Selected:0));
                if(open)openNodeIds_.insert(node.id);else openNodeIds_.erase(node.id);
            }else ImGui::Selectable(node.name.c_str(),selected(index),ImGuiSelectableFlags_SpanAvailWidth);
            observeWidget("node."+node.id);selectionClick(node);
            if(open){self(self,node.id);ImGui::TreePop();}ImGui::PopID();
        }
    };
    if(outlinerFilter_.IsActive()) {
        for(std::size_t index=0;index<nodes.size();++index)if(std::find(visible.begin(),visible.end(),nodes[index].id)!=visible.end()) {
            ImGui::PushID(nodes[index].id.c_str());ImGui::Selectable(nodes[index].name.c_str(),selected(index));
            observeWidget("node."+nodes[index].id);selectionClick(nodes[index]);
            ImGui::SameLine();ImGui::TextDisabled("%s",nodes[index].resourceId.empty()?"Node":"Mesh");ImGui::PopID();
        }
    }else drawNode(drawNode,"");
    ImGui::End();
}


}
#endif
