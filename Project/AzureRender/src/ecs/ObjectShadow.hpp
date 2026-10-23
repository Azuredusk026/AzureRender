#pragma once
#include "reflection/Annotations.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace azurerender::ecs {
AZURE_TYPE("azure.object-shadow", 1)
struct ObjectShadowComponent {
    AZURE_FIELD("启用", 0, 1)
    bool enabled = true;
    AZURE_FIELD("局部中心", -100000, 100000)
    AZURE_FIELD_EDITOR("m", 6, true)
    std::array<float,3> center{0,0,0};
    AZURE_FIELD("光空间半尺寸", 0.0001, 100000)
    AZURE_FIELD_EDITOR("m", 6, true)
    std::array<float,3> halfExtents{1,1,1};
    AZURE_FIELD("法线偏置", 0, 100)
    AZURE_FIELD_EDITOR("m", 8, true)
    float normalBias = .005F;
    AZURE_FIELD("朝光偏置", 0, 100)
    AZURE_FIELD_EDITOR("m", 8, true)
    float lightBias = .0025F;
    AZURE_FIELD("水平过滤半径", 0, 64)
    AZURE_FIELD_EDITOR("px", 4, true)
    float filterRadiusX = 4;
    AZURE_FIELD("垂直过滤半径", 0, 64)
    AZURE_FIELD_EDITOR("px", 4, true)
    float filterRadiusY = 2;
    AZURE_FIELD("图集分区（8 为自动）", 0, 8)
    std::uint32_t atlasPage = 8;
    AZURE_FIELD("接收水平偏移", -2, 2)
    AZURE_FIELD_EDITOR("px", 4, true)
    float receiverPixelOffsetX = 0;
    AZURE_FIELD("接收垂直偏移", -2, 2)
    AZURE_FIELD_EDITOR("px", 4, true)
    float receiverPixelOffsetY = 0;
    AZURE_FIELD("表面法线偏置", 0, 1)
    bool surfaceNormalBias = false;

    void validate() const {
        const auto bounded=[](float value,float minimum,float maximum){
            return std::isfinite(value)&&value>=minimum&&value<=maximum;
        };
        for(float value:center)if(!bounded(value,-100000,100000))throw std::invalid_argument("Object shadow center outside its finite range");
        for(float value:halfExtents)if(!bounded(value,.0001F,100000))throw std::invalid_argument("Object shadow half extents outside their positive range");
        for(float value:{filterRadiusX,filterRadiusY})if(!bounded(value,0,64))throw std::invalid_argument("Object shadow filter radius outside [0,64]");
        for(float value:{receiverPixelOffsetX,receiverPixelOffsetY})if(!bounded(value,-2,2))throw std::invalid_argument("Object shadow receiver pixel offset outside [-2,2]");
        if(!bounded(normalBias,0,100)||!bounded(lightBias,0,100)||atlasPage>8)
            throw std::invalid_argument("Object shadow bias or atlas page outside its range");
    }
};
}
