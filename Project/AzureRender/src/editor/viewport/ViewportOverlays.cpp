#include "editor/ImGuiEditorLayer.hpp"
#ifdef AZURERENDER_HAS_IMGUI
#include "ImViewGuizmo.h"
#include "render/CameraProjection.hpp"
#include <cstdio>
namespace azurerender {
bool ImGuiEditorLayer::drawViewportOrientation(ImVec2 origin,ImVec2 size){
    const auto& io=ImGui::GetIO();const bool enabled=session_->editCameraActive()&&!session_->building()&&!session_->gizmo().active()&&navigationButton_<0&&!io.AppFocusLost&&!io.WantTextInput
        &&!ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId|ImGuiPopupFlags_AnyPopupLevel);
    auto* draw=ImGui::GetWindowDrawList();draw->PushClipRect(origin,{origin.x+size.x,origin.y+size.y},true);
    const auto result=ImViewGuizmo::Draw(session_->viewportView(),{origin.x+size.x-58*dpi_,origin.y+58*dpi_},dpi_,enabled);draw->PopClipRect();
    for(const auto& axis:result.axes)widgets_[std::string("viewport.axis.")+axis.id]={axis.position.x-axis.radius,axis.position.y-axis.radius,2*axis.radius,2*axis.radius};
    if(result.clicked)session_->edit("viewport.orientation",{{"axis",result.clicked}});
    if(result.hovered)ImGui::SetTooltip("Click an axis for an orthographic view. View menu provides all six directions.");
    return result.hovered;
}
void ImGuiEditorLayer::drawViewportOverlays(ImVec2 origin,ImVec2 size){
    if(!session_->editCameraActive())return;
    auto* draw=ImGui::GetWindowDrawList();draw->PushClipRect(origin,{origin.x+size.x,origin.y+size.y},true);
    try{
        const auto scene=session_->viewScene();
        std::vector<ViewportOverlayLine> skeleton;const auto policy=session_->viewportOverlayPolicies().find("skeleton");if(policy!=session_->viewportOverlayPolicies().end()&&policy->second!="hidden")skeleton=session_->animations().skeletonLines(session_->context());
        const auto lines=session_->viewportOverlays().collect({session_->context(),session_->viewportView(),&scene,&skeleton},session_->viewportOverlayPolicies(),session_->selection().selected());
        for(const auto& line:lines){const auto segment=projectCameraSegment(session_->viewportView(),size.x/size.y,line.from,line.to);if(!segment)continue;
            const auto color=ImGui::ColorConvertFloat4ToU32({line.color[0],line.color[1],line.color[2],line.color[3]});
            draw->AddLine({origin.x+segment->from[0]*size.x,origin.y+segment->from[1]*size.y},{origin.x+segment->to[0]*size.x,origin.y+segment->to[1]*size.y},color,std::max(1.F,dpi_));
        }
    }catch(const std::exception& error){session_->log(std::string("Viewport helpers: ")+error.what());}
    draw->PopClipRect();
}
}
#endif
