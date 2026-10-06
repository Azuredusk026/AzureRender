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
void ImGuiEditorLayer::drawAnimationPanel(PanelContext& panelContext){
    const auto& view=panelContext.view();
    ImGui::SetNextWindowSize({360,260},ImGuiCond_FirstUseEver);
    if(!ImGui::Begin("Animation Preview###animation",workspace_.open("animation"))){ImGui::End();return;}
    if(view.isProject() && view.selectedNode()){
        try{
            const auto data=view.componentData(view.selectedNode()->id,"azure.animator");
            if(!data.is_null() && !data.at("asset").get<std::string>().empty()){
                std::ifstream input(view.assets().resolveReference(data.at("asset").get<std::string>()));nlohmann::json graph;input>>graph;
                static std::string selected,previous,node;static float time=0,crossfade=.18F;
                if(node!=view.selectedNode()->id){node=view.selectedNode()->id;selected=data.at("state").get<std::string>();previous=selected;time=0;}
                ImGui::BeginDisabled(session_->playing() || session_->building());
                bool changed=false;
                if(ImGui::BeginCombo("Semantic state",selected.c_str())){
                    for(const auto& state:graph.at("states")){
                        const auto name=state.at("name").get<std::string>();
                        if(ImGui::Selectable(name.c_str(),name==selected)){previous=selected;selected=name;time=0;changed=true;}
                    }ImGui::EndCombo();
                }
                changed=ImGui::DragFloat("Time (seconds)",&time,.01F,0,600)||changed;
                changed=ImGui::SliderFloat("Crossfade (seconds)",&crossfade,0,2)||changed;
                if(ui::button("Preview pose") || changed)session_->edit("animation.preview",{{"state",selected},{"time",time},{"previous",previous},{"crossfade",crossfade}});
                ImGui::SameLine();if(ui::button("Clear preview"))session_->edit("animation.clear-preview");
                if(ui::button("Set initial semantic"))session_->edit("component.field",{{"type","azure.animator"},{"field","state"},{"value",selected}});
                ImGui::EndDisabled();
                for(const auto& state:graph.at("states"))ImGui::Text("%s: clip %u, %s",state.at("name").get<std::string>().c_str(),state.at("clip").get<unsigned>(),state.value("loop",true)?"loop":"once");
                if(const auto& preview=view.animationPreview();preview)ImGui::Text("Clip %u, time %.3f, blend %.3f",preview->clip,preview->time,preview->blend);
            }else ImGui::TextUnformatted("Select a node with a configured animator.");
        }catch(const std::exception& error){ImGui::TextWrapped("%s",error.what());}
    }
    ImGui::End();
}

}
#endif
