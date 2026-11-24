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
std::vector<unsigned char> brandPixels(int size){
    if(size<1||size>256)return {};
    std::vector<unsigned char> pixels(static_cast<std::size_t>(size)*size*4,0);
    for(int y=0;y<size;++y)for(int x=0;x<size;++x){
        const float px=(x+.5F)*64/size,py=(y+.5F)*64/size;
        const float half=(py-4)*30/56;
        const bool outer=py>=4&&py<=60&&px>=32-half&&px<=32+half;
        const bool inner=py>=24&&py<40&&px>32-(py-24)*.5F&&px<32+(py-24)*.5F;
        const bool lower=py>45&&px>25-(py-45)*8/15&&px<39+(py-45)*8/15;
        if(outer&&!inner&&!lower){const auto index=(static_cast<std::size_t>(y)*size+x)*4;pixels[index]=99;pixels[index+1]=179;pixels[index+2]=255;pixels[index+3]=255;}
    }
    return pixels;
}

}
