#include "BrandMark.hpp"
#include "ThemeTokens.hpp"
namespace azurerender::ui {
void brandMark(float size){
    const auto p=ImGui::GetCursorScreenPos();
    const auto point=[&](float x,float y){return ImVec2(p.x+x*size/64,p.y+y*size/64);};
    const auto color=ImGui::ColorConvertFloat4ToU32(ThemeTokens::dark().accent);
    auto* draw=ImGui::GetWindowDrawList();
    draw->AddQuadFilled(point(32,4),point(32,24),point(17,60),point(2,60),color);
    draw->AddQuadFilled(point(32,4),point(62,60),point(47,60),point(32,24),color);
    draw->AddQuadFilled(point(24,40),point(40,40),point(39,45),point(25,45),color);
    ImGui::Dummy({size,size});
}
}
