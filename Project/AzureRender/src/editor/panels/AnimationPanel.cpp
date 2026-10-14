#include "editor/ImGuiEditorLayer.hpp"
#include "ui/Widgets.hpp"
#include "ui/UiScopes.hpp"
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
    if(!ImGui::Begin("动画预览###animation",workspace_.open("animation"))){ImGui::End();return;}
    if(view.isProject() && view.selectedNode()){
        try{
            const auto data=view.componentData(view.selectedNode()->id,"azure.animator");
            if(!data.is_null() && !data.at("asset").get<std::string>().empty()){
                std::ifstream input(view.assets().resolveReference(data.at("asset").get<std::string>()));nlohmann::json graph;input>>graph;
                const auto node=view.selectedNode()->id;
                const auto timelines=session_->animations().describe();
                const auto timeline=timelines.value(node,nlohmann::json::object());
                auto selected=timeline.value("state",data.at("state").get<std::string>()),previous=timeline.value("previous",std::string());
                float time=timeline.value("time",0.F),crossfade=timeline.value("crossfade",.18F);
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
                if(ui::button("Preview pose") || changed)session_->edit("animation.seek",{{"node",node},{"state",selected},{"time",time},{"previous",previous},{"crossfade",crossfade}});observeWidget("animation.seek");
                ImGui::SameLine();if(ui::button("Clear preview"))session_->edit("animation.clear",{{"node",node}});observeWidget("animation.clear");
                if(ui::button(timeline.value("playing",false)?"Pause animation":"Play animation"))session_->edit(timeline.value("playing",false)?"animation.pause":"animation.play",{{"node",node}});observeWidget("animation.play-pause");
                float speed=timeline.value("speed",1.F);bool loop=timeline.value("loop",true);
                const bool rateChanged=ImGui::SliderFloat("Playback speed",&speed,.05F,4);observeWidget("animation.speed");
                const bool loopChanged=ImGui::Checkbox("Loop preview",&loop);observeWidget("animation.loop");
                if(rateChanged||loopChanged)session_->edit("animation.configure",{{"node",node},{"speed",speed},{"loop",loop}});
                bool skeleton=session_->viewportOverlayPolicies().count("skeleton")&&session_->viewportOverlayPolicies().at("skeleton")=="selected";
                if(ImGui::Checkbox("Show selected skeleton",&skeleton))session_->edit("viewport.overlay-policy",{{"category","skeleton"},{"policy",skeleton?"selected":"hidden"}});observeWidget("animation.skeleton");
                if(ui::button("Set initial semantic"))session_->edit("component.field",{{"type","azure.animator"},{"field","state"},{"value",selected}});
                ImGui::EndDisabled();
                for(const auto& state:graph.at("states"))ImGui::Text("%s: clip %u, %s",state.at("name").get<std::string>().c_str(),state.at("clip").get<unsigned>(),state.value("loop",true)?"loop":"once");
                if(!timeline.empty())ImGui::Text("Clip %u, time %.3f, blend %.3f",timeline.at("clip").get<unsigned>(),timeline.at("time").get<double>(),timeline.at("blend").get<float>());
            }else ImGui::TextUnformatted("Select a node with a configured animator.");
        }catch(const std::exception& error){ImGui::TextWrapped("%s",error.what());}
    }
    ImGui::End();
}

}
#endif
