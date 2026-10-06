#pragma once
#include <cmath>
#include <stdexcept>
namespace azurerender::ui {
struct UiMetrics {
    float scale,controlHeight,spacing,padding,menuHeight,toolbarHeight,statusHeight,propertyLabelWidth;
    static UiMetrics fromScale(float scale) {
        if(!std::isfinite(scale)||scale<.75F||scale>3.F)throw std::invalid_argument("UI scale must be between 0.75 and 3");
        return {scale,28*scale,8*scale,8*scale,24*scale,40*scale,24*scale,110*scale};
    }
};
}
