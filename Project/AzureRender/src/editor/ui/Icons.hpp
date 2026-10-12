#pragma once
#include "ThemeTokens.hpp"
#include <algorithm>
namespace azurerender::ui {
enum class Icon { Project, Success, Error };
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
