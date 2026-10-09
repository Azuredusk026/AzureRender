#include "editor/ImGuiEditorLayer.hpp"
#ifdef AZURERENDER_HAS_IMGUI
#include <imgui.h>
namespace azurerender {
void ImGuiEditorLayer::drawPhysicsAuthoring(const SceneNode& node){
    const auto body=context_->componentData(node.id,"azure.rigid-body");
    const auto hinge=context_->componentData(node.id,"azure.hinge");
    if(body.is_null()&&hinge.is_null())return;
    if(!ImGui::CollapsingHeader("Physics authoring",ImGuiTreeNodeFlags_DefaultOpen))return;
    if(!body.is_null()){
        const auto shape=body.at("shape").get<std::string>();
        ImGui::TextUnformatted("Collision shape");ImGui::SetNextItemWidth(-1);
        if(ImGui::BeginCombo("##physics-shape",shape.c_str())){
            for(const auto* value:{"box","sphere","capsule","mesh"}){
                const bool unavailable=std::string(value)=="mesh"&&(body.at("mesh").get<std::string>().empty()||body.at("dynamic").get<bool>()||body.at("trigger").get<bool>());
                ImGui::BeginDisabled(unavailable);
                if(ImGui::Selectable(value,shape==value))session_->edit("component.fields",{{"type","azure.rigid-body"},{"values",{{"shape",value}}}});
                observeWidget(std::string("physics.shape.")+value);ImGui::EndDisabled();
            }ImGui::EndCombo();
        }observeWidget("physics.shape");
        ImGui::TextWrapped("Set dimensions below. Mesh requires a static nonsensor body and a model reference. Sphere uses uniform scale; capsule uses equal X/Z scale.");
    }
    if(!hinge.is_null()){
        ImGui::TextUnformatted("World hinge axis");
        for(unsigned axis=0;axis<3;++axis){std::array<float,3> direction{},normal{};direction[axis]=1;normal[(axis+1)%3]=1;
            const char* title=axis==0?"X":axis==1?"Y":"Z";
            if(ImGui::Button(title))session_->edit("component.fields",{{"type","azure.hinge"},{"values",{{"axis",direction},{"normal",normal}}}});
            observeWidget(std::string("physics.axis.")+title);if(axis<2)ImGui::SameLine();
        }
        ImGui::TextWrapped("Pick two rigid body references below, with at least one dynamic body. Anchors use each body's local origin. Angle limits bracket zero.");
    }
    ImGui::TextWrapped("Simulate runs physics with the editor camera. Pause, Step and Stop share the preview lifecycle. View > Helpers controls collision and hinge visualization.");
}
}
#endif
