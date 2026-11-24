#pragma once
#include "reflection/Annotations.hpp"
#include <array>
#include <string>
namespace azurerender::game {
AZURE_TYPE("azure.reference-camera", 1)
struct ReferenceCamera {
    AZURE_FIELD("启用参考视图", 0, 1)
    bool enabled=false;
    AZURE_FIELD("参考视图名称", 0, 0)
    std::string view;
    AZURE_FIELD("锚点位置", -1000000, 1000000)
    std::array<float,3> anchorPosition{0,0,0};
    AZURE_FIELD("锚点右轴", -1, 1)
    std::array<float,3> anchorRight{1,0,0};
    AZURE_FIELD("锚点上轴", -1, 1)
    std::array<float,3> anchorUp{0,1,0};
    AZURE_FIELD("锚点后轴", -1, 1)
    std::array<float,3> anchorBack{0,0,1};
};
}
