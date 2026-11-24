#pragma once
// Direction drawing adapted from ImViewGuizmo by Marcel Kazemi.
// Copyright (c) 2025 Marcel Kazemi. MIT; see LICENSE.txt and source.json.
// Azure adapter uses engine matrices and emits axis intents; camera ownership
// and input capture remain in the engine's production operation service.
#include <imgui.h>
#include "render/CameraProjection.hpp"
#include <algorithm>
#include <array>
namespace ImViewGuizmo {
struct AxisHit {const char* id;ImVec2 position;float radius,depth;unsigned axis;};
struct Result {bool hovered=false;const char* clicked=nullptr;std::array<AxisHit,6> axes;};
inline Result Draw(const azurerender::CameraViewDescriptor& view,ImVec2 center,float scale,bool enabled){
    const auto matrix=azurerender::cameraViewMatrix(view);Result result;
    const char* names[]={"+X","-X","+Y","-Y","+Z","-Z"};
    for(unsigned i=0;i<6;++i){const unsigned axis=i/2;const float sign=i%2?-1.F:1.F;const float depth=matrix[axis*4+2]*sign;
        result.axes[i]={names[i],{center.x+matrix[axis*4]*sign*34*scale,center.y-matrix[axis*4+1]*sign*34*scale},(depth<0?7.F:10.F)*scale,depth,axis};
    }
    auto sorted=result.axes;std::sort(sorted.begin(),sorted.end(),[](const auto& a,const auto& b){return a.depth<b.depth;});
    auto* draw=ImGui::GetWindowDrawList();const ImU32 colors[]={IM_COL32(230,80,80,255),IM_COL32(80,220,110,255),IM_COL32(90,140,230,255)};
    draw->AddCircleFilled(center,52*scale,IM_COL32(20,23,29,160));
    const auto mouse=ImGui::GetMousePos();const char* hovered=nullptr;
    for(const auto& axis:sorted){const auto dx=mouse.x-axis.position.x,dy=mouse.y-axis.position.y;const bool hit=enabled&&dx*dx+dy*dy<axis.radius*axis.radius;
        auto color=ImGui::ColorConvertU32ToFloat4(colors[axis.axis]);color.w=axis.depth<0?.4F:1.F;const auto packed=ImGui::ColorConvertFloat4ToU32(color);
        draw->AddLine(center,axis.position,packed,2*scale);draw->AddCircleFilled(axis.position,axis.radius,packed);
        if(axis.depth>=0){const auto size=ImGui::CalcTextSize(axis.id);draw->AddText({axis.position.x-size.x*.5F,axis.position.y-size.y*.5F},IM_COL32(20,23,29,255),axis.id);}
        if(hit){hovered=axis.id;draw->AddCircle(axis.position,axis.radius+2*scale,IM_COL32(255,245,170,255),0,2*scale);}
    }
    const float dx=mouse.x-center.x,dy=mouse.y-center.y;result.hovered=enabled&&dx*dx+dy*dy<52*52*scale*scale;
    if(hovered&&ImGui::IsMouseClicked(ImGuiMouseButton_Left))result.clicked=hovered;
    return result;
}
}
