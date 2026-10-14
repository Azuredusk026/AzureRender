#pragma once
#include <imgui.h>
namespace azurerender::ui {
struct ThemeTokens {
    static ImVec4 axis(int index) {
        const ImVec4 colors[]={{.70F,.18F,.18F,1},{.18F,.55F,.25F,1},{.18F,.36F,.70F,1}};
        return colors[index];
    }
    ImVec4 text, muted, surface, recessed, border, control, hover, accent, selected, error, success, warning;
    static ThemeTokens dark() {
        auto rgb=[](int r,int g,int b){return ImVec4(r/255.F,g/255.F,b/255.F,1);};
        return {rgb(231,231,231),rgb(172,172,172),rgb(27,27,27),rgb(20,20,20),rgb(57,57,57),
            rgb(44,44,44),rgb(52,52,52),rgb(99,179,255),rgb(38,62,83),rgb(242,133,133),rgb(121,201,154),rgb(232,187,107)};
    }
};
}
