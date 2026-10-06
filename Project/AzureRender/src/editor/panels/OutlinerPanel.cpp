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
    // Recursive tree draw: children are nodes whose parentId equals the
    // current node id. Use an index-based recursion to avoid iterator
    // invalidation while nodes stay stable during a frame.
    const auto drawNode = [&](const auto& self,
                              const std::string& parentId,
                              const int depth) -> void {
        for (std::size_t index = 0; index < nodes.size(); ++index) {
            if (nodes[index].parentId != parentId) {
                continue;
            }
            const bool selected = std::find(view.selectedNodes().begin(),view.selectedNodes().end(),index)!=view.selectedNodes().end();
            bool hasChildren = false;
            for (const SceneNode& candidate : nodes) {
                if (candidate.parentId == nodes[index].id) {
                    hasChildren = true;
                    break;
                }
            }
            ImGui::PushID(static_cast<int>(index));
            bool open = false;
            if (hasChildren) {
                const bool clicked = ImGui::TreeNodeEx(
                    nodes[index].name.c_str(),
                    ImGuiTreeNodeFlags_OpenOnArrow
                        | ImGuiTreeNodeFlags_SpanAvailWidth
                        | (selected ? ImGuiTreeNodeFlags_Selected : 0));
                open = clicked;
            } else {
                ImGui::Selectable(
                    nodes[index].name.c_str(),
                    selected,
                    ImGuiSelectableFlags_SpanAvailWidth);
            }
            observeWidget("node."+nodes[index].id);
            if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
                if(ImGui::GetIO().KeyCtrl){auto selection=panelContext.selection().selected();auto found=std::find(selection.begin(),selection.end(),nodes[index].id);if(found==selection.end())selection.push_back(nodes[index].id);else selection.erase(found);panelContext.selection().set(selection);}else panelContext.selection().set({nodes[index].id});
            }
            if (open) {
                self(self, nodes[index].id, depth + 1);
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
    };
    if(outlinerFilter_.IsActive()) {
        for(std::size_t i=0;i<nodes.size();++i)if(outlinerFilter_.PassFilter(nodes[i].name.c_str()) || outlinerFilter_.PassFilter(nodes[i].id.c_str())) {
            ImGui::PushID(nodes[i].id.c_str());
            if(ImGui::Selectable(nodes[i].name.c_str(),view.selectedNodeIndex()==i))panelContext.selection().set({nodes[i].id});
            observeWidget("node."+nodes[i].id);ImGui::SameLine();ImGui::TextDisabled("%s",nodes[i].resourceId.empty()?"Node":"Mesh");ImGui::PopID();
        }
    }else drawNode(drawNode, "", 0);
    ImGui::End();
}


}
#endif
