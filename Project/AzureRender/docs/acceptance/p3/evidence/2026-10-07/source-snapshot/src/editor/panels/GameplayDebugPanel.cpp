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
void ImGuiEditorLayer::drawGameplayDebugPanel(PanelContext& panelContext){
    const auto& view=panelContext.view();
    ImGui::SetNextWindowSize({370,280},ImGuiCond_FirstUseEver);
    if(!ImGui::Begin("Gameplay Debug###gameplay-debug",workspace_.open("gameplay-debug"))){ImGui::End();return;}
    auto overlay=session_->debugOverlay;
    if(ImGui::Checkbox("Collision and camera overlay",&overlay))session_->edit("viewport.debug-overlay",{{"enabled",overlay}});
    const auto scene=session_->viewScene();auto* runtime=session_->runtime();auto* game=session_->game();
    for(std::size_t index=0;index<scene.nodes.size();++index){const auto& node=scene.nodes[index];
        const auto entity=runtime?runtime->entity(node.id):ecs::kInvalidEntity;
        const bool character=runtime?runtime->world().has<game::Character>(entity):!view.componentData(node.id,"azure.character").is_null();
        const bool body=runtime?runtime->world().has<game::RigidBody>(entity):!view.componentData(node.id,"azure.rigid-body").is_null();
        if(!character&&!body)continue;
        ImGui::PushID(node.id.c_str());
        if(ImGui::Selectable(node.id.c_str(),view.selectedNode() && view.selectedNode()->id==node.id)){
            for(std::size_t edit=0;edit<view.scene().nodes.size();++edit)if(view.scene().nodes[edit].id==node.id){session_->edit("node.select",{{"index",edit}});break;}
        }
        if(character && game){const auto v=game->physics().velocity(entity);ImGui::Text("Velocity %.2f %.2f %.2f, grounded %s",v[0],v[1],v[2],game->physics().grounded(entity)?"yes":"no");}
        ImGui::PopID();
    }
    if(game && game->hasCamera()){
        const auto& camera=game->camera();ImGui::Text("Camera distance %.2f / %.2f",camera.actualDistance(),camera.distance());
        const auto target=camera.target();ImGui::Text("Camera target %.2f %.2f %.2f",target[0],target[1],target[2]);
    }
    if(game && game->interactionTarget()){
        const auto& target=*game->interactionTarget();ImGui::Text("Interaction: %s",target.node.c_str());
        if(ui::button("Locate interaction"))for(std::size_t i=0;i<view.scene().nodes.size();++i)if(view.scene().nodes[i].id==target.node){session_->edit("node.select",{{"index",i}});break;}
    }
    if(auto* scripts=session_->scripts())for(const auto& error:scripts->errors()){
        ImGui::TextWrapped("%s",error.c_str());
        for(std::size_t i=0;i<view.scene().nodes.size();++i){const auto& node=view.scene().nodes[i];const auto script=view.componentData(node.id,"azure.script");
            if(!script.is_null() && error.find(script.at("asset").get<std::string>())!=std::string::npos){ImGui::PushID(node.id.c_str());if(ImGui::SmallButton(("Locate "+node.id).c_str()))session_->edit("node.select",{{"index",i}});ImGui::PopID();}}
    }
    ImGui::End();
}


}
#endif
