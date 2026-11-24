#pragma once
#include <imgui.h>
namespace azurerender::ui {
struct ThemeTokens {
    ImVec4 text, muted, surface, recessed, border, control, hover, accent, selected, error, success, warning;
    static ThemeTokens dark() {
        auto rgb=[](int r,int g,int b){return ImVec4(r/255.F,g/255.F,b/255.F,1);};
        return {rgb(230,233,239),rgb(168,176,188),rgb(32,35,41),rgb(21,23,27),rgb(52,57,65),
            rgb(44,49,58),rgb(57,86,119),rgb(77,163,255),rgb(46,66,90),rgb(255,140,115),rgb(110,211,150),rgb(230,186,91)};
    }
};
}
