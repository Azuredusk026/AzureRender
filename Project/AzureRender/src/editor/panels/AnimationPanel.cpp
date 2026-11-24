#include "ui/localization/Localization.hpp"
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
    if(!ImGui::Begin(ui::tr("动画预览###animation"),workspace_.open("animation"))){ImGui::End();return;}
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
                if(ImGui::BeginCombo(ui::tr("Semantic state"),selected.c_str())){
                    for(const auto& state:graph.at("states")){
                        const auto name=state.at("name").get<std::string>();
                        if(ImGui::Selectable(name.c_str(),name==selected)){previous=selected;selected=name;time=0;changed=true;}
                    }ImGui::EndCombo();
                }
                changed=ImGui::DragFloat(ui::tr("Time (seconds)"),&time,.01F,0,600)||changed;
                changed=ImGui::SliderFloat(ui::tr("Crossfade (seconds)"),&crossfade,0,2)||changed;
                if(ui::button(ui::tr("Preview pose")) || changed)session_->edit("animation.seek",{{"node",node},{"state",selected},{"time",time},{"previous",previous},{"crossfade",crossfade}});observeWidget("animation.seek");
                ImGui::SameLine();if(ui::button(ui::tr("Clear preview")))session_->edit("animation.clear",{{"node",node}});observeWidget("animation.clear");
                if(ui::button(timeline.value("playing",false)?ui::tr("Pause animation"):ui::tr("Play animation")))session_->edit(timeline.value("playing",false)?"animation.pause":"animation.play",{{"node",node}});observeWidget("animation.play-pause");
                float speed=timeline.value("speed",1.F);bool loop=timeline.value("loop",true);
                const bool rateChanged=ImGui::SliderFloat(ui::tr("Playback speed"),&speed,.05F,4);observeWidget("animation.speed");
                const bool loopChanged=ImGui::Checkbox(ui::tr("Loop preview"),&loop);observeWidget("animation.loop");
                if(rateChanged||loopChanged)session_->edit("animation.configure",{{"node",node},{"speed",speed},{"loop",loop}});
                bool skeleton=session_->viewportOverlayPolicies().count("skeleton")&&session_->viewportOverlayPolicies().at("skeleton")=="selected";
                if(ImGui::Checkbox(ui::tr("Show selected skeleton"),&skeleton))session_->edit("viewport.overlay-policy",{{"category","skeleton"},{"policy",skeleton?"selected":"hidden"}});observeWidget("animation.skeleton");
                if(ui::button(ui::tr("Set initial semantic")))session_->edit("component.field",{{"type","azure.animator"},{"field","state"},{"value",selected}});
                const auto expression=data.value("expression",std::string());
                if(!expression.empty()){
                    const auto preset=nlohmann::json::parse(expression);
                    ImGui::Text("%s",preset.at("id").get<std::string>().c_str());
                    float weight=data.value("expressionWeight",1.F);
                    if(ImGui::SliderFloat(ui::tr("表情强度"),&weight,0,1))session_->edit("animation.expression.weight",{{"weight",weight}});
                    observeWidget("animation.expression.weight");
                    if(ui::button(ui::tr("清除表情")))session_->edit("animation.expression.clear",{});
                    observeWidget("animation.expression.clear");
                }
                ImGui::EndDisabled();
                for(const auto& state:graph.at("states"))ImGui::Text(ui::tr("%s: clip %u, %s"),state.at("name").get<std::string>().c_str(),state.at("clip").get<unsigned>(),state.value("loop",true)?ui::tr("loop"):ui::tr("once"));
                if(!timeline.empty())ImGui::Text(ui::tr("Clip %u, time %.3f, blend %.3f"),timeline.at("clip").get<unsigned>(),timeline.at("time").get<double>(),timeline.at("blend").get<float>());
            }else ImGui::TextUnformatted(ui::tr("Select a node with a configured animator."));
        }catch(const std::exception& error){ImGui::TextWrapped("%s",error.what());}
    }
    ImGui::End();
}

}
#endif
