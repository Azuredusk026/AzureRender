#pragma once
#include "ThemeTokens.hpp"
#include <algorithm>
namespace azurerender::ui {
enum class Icon { Project, Success, Error, File, Search, Create, Move, Rotate, Scale, Play, Pause, Stop, Settings, Publish };
inline void icon(Icon value,float size=0) {
    if(size<=0)size=ImGui::GetFontSize();
    const auto p=ImGui::GetCursorScreenPos();const auto theme=ThemeTokens::dark();
    const auto color=ImGui::ColorConvertFloat4ToU32(value==Icon::Error?theme.error:value==Icon::Success?theme.success:theme.accent);
    auto* draw=ImGui::GetWindowDrawList();const auto point=[&](float x,float y){return ImVec2(p.x+x*size,p.y+y*size);};
    const auto thickness=std::max(1.F,size*.07F);
    if(value==Icon::Project) {
        draw->AddRect(point(.08F,.28F),point(.92F,.85F),color,size*.08F,0,thickness);
        draw->AddLine(point(.12F,.28F),point(.12F,.15F),color,thickness);
        draw->AddLine(point(.12F,.15F),point(.48F,.15F),color,thickness);
        draw->AddLine(point(.48F,.15F),point(.60F,.28F),color,thickness);
    }else if(value==Icon::Search) {
        draw->AddCircle(point(.40F,.40F),size*.28F,color,0,thickness);
        draw->AddLine(point(.62F,.62F),point(.92F,.92F),color,thickness);
    }else if(value==Icon::Play) {
        draw->AddTriangle(point(.25F,.15F),point(.85F,.50F),point(.25F,.85F),color,thickness);
    }else if(value==Icon::Pause) {
        draw->AddLine(point(.3F,.2F),point(.3F,.8F),color,thickness*2);
        draw->AddLine(point(.7F,.2F),point(.7F,.8F),color,thickness*2);
    }else if(value==Icon::Create||value==Icon::Move) {
        draw->AddLine(point(.5F,.1F),point(.5F,.9F),color,thickness);
        draw->AddLine(point(.1F,.5F),point(.9F,.5F),color,thickness);
        if(value==Icon::Move){draw->AddTriangleFilled(point(.5F,0),point(.35F,.2F),point(.65F,.2F),color);}
    }else if(value==Icon::Rotate||value==Icon::Settings) {
        draw->AddCircle(point(.5F,.5F),size*.34F,color,12,thickness);
        if(value==Icon::Settings)draw->AddCircle(point(.5F,.5F),size*.12F,color,0,thickness);
        else draw->AddTriangleFilled(point(.85F,.2F),point(.85F,.48F),point(.6F,.3F),color);
    }else if(value==Icon::Publish) {
        draw->AddLine(point(.5F,.15F),point(.5F,.72F),color,thickness);
        draw->AddLine(point(.25F,.4F),point(.5F,.15F),color,thickness);
        draw->AddLine(point(.75F,.4F),point(.5F,.15F),color,thickness);
        draw->AddLine(point(.15F,.85F),point(.85F,.85F),color,thickness);
    }else if(value==Icon::File||value==Icon::Stop||value==Icon::Scale) {
        draw->AddRect(point(.2F,.15F),point(.8F,.85F),color,0,0,thickness);
        if(value==Icon::Scale)draw->AddLine(point(.2F,.85F),point(.8F,.15F),color,thickness);
    }else if(value==Icon::Success) {
        draw->AddLine(point(.15F,.50F),point(.40F,.76F),color,thickness);
        draw->AddLine(point(.40F,.76F),point(.85F,.22F),color,thickness);
    }else {
        draw->AddCircle(point(.50F,.50F),size*.40F,color,0,thickness);
        draw->AddLine(point(.50F,.25F),point(.50F,.55F),color,thickness);
        draw->AddCircleFilled(point(.50F,.72F),size*.04F,color);
    }
    ImGui::Dummy({size,size});
}
}
