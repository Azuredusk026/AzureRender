#include "editor/ImGuiEditorLayer.hpp"
#include "scene/TransformEditing.hpp"
#include "ImGuizmo.h"
#include <cmath>
namespace azurerender {
void ImGuiEditorLayer::cancelViewportGizmo() {
    if(session_->gizmo().active())session_->edit("viewport.gizmo-cancel");
    viewportGizmoDragActive_=false;
    ImGuizmo::Enable(false);
}
bool ImGuiEditorLayer::drawViewportGizmo(ImVec2 origin,ImVec2 size) {
    const auto& io=ImGui::GetIO();
    const bool enabled=!session_->playing()&&!session_->building()&&context_->selectedNode()
        &&context_->gizmoMode()!=EditorContext::GizmoMode::Select&&!io.AppFocusLost
        &&navigationButton_<0&&!io.WantTextInput
        &&!ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId|ImGuiPopupFlags_AnyPopupLevel);
    if(!enabled || ImGui::IsKeyPressed(ImGuiKey_Escape,false)) {
        cancelViewportGizmo();return false;
    }
    ImGuizmo::Enable(true);ImGuizmo::SetOrthographic(false);ImGuizmo::AllowAxisFlip(false);
    auto& style=ImGuizmo::GetStyle();style=ImGuizmo::Style{};
    style.TranslationLineThickness*=dpi_;style.TranslationLineArrowSize*=dpi_;
    style.RotationLineThickness*=dpi_;style.ScaleLineThickness*=dpi_;
    style.ScaleLineCircleSize*=dpi_;style.CenterCircleSize*=dpi_;
    style.Colors[ImGuizmo::DIRECTION_X]={230/255.F,80/255.F,80/255.F,1};
    style.Colors[ImGuizmo::DIRECTION_Y]={80/255.F,220/255.F,110/255.F,1};
    style.Colors[ImGuizmo::DIRECTION_Z]={90/255.F,140/255.F,230/255.F,1};
    ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
    ImGuizmo::SetRect(origin.x,origin.y,size.x,size.y);
    const float handleSize=100.F*dpi_/size.x;
    ImGuizmo::SetGizmoSizeClipSpace(handleSize);
    auto projection=context_->viewportProjection();projection[5]*=-1;
    const auto& view=context_->viewportView();
    const GizmoDragRequest request{context_->gizmoSpace()==EditorContext::GizmoSpace::World?"world":"local",
        context_->gizmoPivot()==EditorContext::GizmoPivot::Active?"active":"bounds"};
    if(!session_->gizmo().active()) {
        try{viewportGizmoMatrix_=session_->gizmo().pivotMatrix(request);}
        catch(const std::exception&){return false;}
    }
    const auto mode=context_->gizmoMode();
    const auto operation=mode==EditorContext::GizmoMode::Translate?ImGuizmo::TRANSLATE:
        mode==EditorContext::GizmoMode::Rotate?ImGuizmo::ROTATE:ImGuizmo::SCALE;
    const auto coordinate=request.space=="world"?ImGuizmo::WORLD:ImGuizmo::LOCAL;
    const auto base=viewportGizmoMatrix_;
    float step=session_->settings().get(mode==EditorContext::GizmoMode::Translate?"editor.gizmo.moveStep":
        mode==EditorContext::GizmoMode::Rotate?"editor.gizmo.rotateStep":"editor.gizmo.scaleStep").get<float>();
    float snap[3]{step,step,step};
    const bool snapped=session_->settings().get("editor.gizmo.snap").get<bool>()||io.KeyCtrl;
    const bool changed=ImGuizmo::Manipulate(view.data(),projection.data(),operation,coordinate,
        viewportGizmoMatrix_.data(),nullptr,snapped?snap:nullptr);
    const bool usingNow=ImGuizmo::IsUsing();
    const bool over=ImGuizmo::IsOver();
    if(usingNow && !session_->gizmo().active()) {
        const auto result=session_->edit("viewport.gizmo-begin");
        if(!result){cancelViewportGizmo();return true;}
    }
    if(changed && session_->gizmo().active()) {
        const auto result=session_->edit("viewport.gizmo-update",{{"matrix",viewportGizmoMatrix_}});
        if(!result){viewportGizmoMatrix_=base;}
    }
    if(!usingNow && session_->gizmo().active())session_->edit("viewport.gizmo-commit");
    viewportGizmoDragActive_=session_->gizmo().active();

    // Observation targets are positions on the actual projected handles.
    const internal::Vector3 center{base[12],base[13],base[14]};
    const auto invView=scene::inverseAffine(view);
    const internal::Vector3 right{invView[0],invView[1],invView[2]};
    const auto screen=[&](const internal::Vector3& point)->ImVec2 {
        const auto projected=context_->projectDebugPoint(point);
        return projected?ImVec2{origin.x+(*projected)[0]*size.x,origin.y+(*projected)[1]*size.y}:ImVec2{-10000,-10000};
    };
    const auto c=screen(center),r=screen(internal::addVectors(center,right));
    const float rightLength=2*std::hypot((r.x-c.x)/size.x,(r.y-c.y)/size.x);
    if(rightLength>1e-7F) {
        const float factor=handleSize/rightLength;
        widgets_["gizmo.center"]={c.x-1,c.y-1,2,2};
        const bool local=request.space=="local";
        for(unsigned axis=0;axis<3;++axis) {
            internal::Vector3 point{};
            if(mode==EditorContext::GizmoMode::Rotate) {
                // Midpoint of the camera-facing semicircle, away from axis intersections.
                point[(axis+1)%3]=.70710678F*factor*1.2F;
                point[(axis+2)%3]=.70710678F*factor*1.2F;
            }else point[axis]=factor;
            if(local) {
                internal::Vector3 transformed{};
                for(unsigned row=0;row<3;++row)for(unsigned column=0;column<3;++column)transformed[row]+=base[column*4+row]*point[column];
                point=transformed;
            }
            const auto p=screen(internal::addVectors(center,point));
            widgets_["gizmo."+std::to_string(axis)]={p.x-3*dpi_,p.y-3*dpi_,6*dpi_,6*dpi_};
            const char* label=axis==0?"X":axis==1?"Y":"Z";
            ImGui::GetWindowDrawList()->AddText({p.x+8*dpi_,p.y-8*dpi_},ImGui::ColorConvertFloat4ToU32(style.Colors[ImGuizmo::DIRECTION_X+axis]),label);
        }
    }
    return over||usingNow;
}
}
