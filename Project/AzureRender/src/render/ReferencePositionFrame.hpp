#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <type_traits>
namespace azurerender {
struct alignas(16) ReferencePositionFrameParameters {
    std::array<std::int32_t,4> axes{1,2,3,0};
    std::array<float,4> offset{};
};
static_assert(sizeof(ReferencePositionFrameParameters)==32);
static_assert(std::is_standard_layout_v<ReferencePositionFrameParameters>);
struct ReferencePositionFrame {
    std::uint32_t schemaVersion=1;
    std::array<std::uint32_t,3> reserved{};
    ReferencePositionFrameParameters parameters;
};
inline void validateReferencePositionFrame(const ReferencePositionFrame& frame){
    if(frame.schemaVersion!=1)throw std::invalid_argument("Unsupported reference position frame version");
    for(auto value:frame.reserved)if(value)throw std::invalid_argument("Reference position frame header reserves must be zero");
    if(frame.parameters.axes[3]||frame.parameters.offset[3]!=0)throw std::invalid_argument("Reference position frame parameter reserves must be zero");
    unsigned used=0;
    for(unsigned i=0;i<3;++i){
        const auto axis=frame.parameters.axes[i];
        if(axis==0||axis < -3||axis>3)throw std::invalid_argument("Reference position frame axes must be signed indices1..3");
        const auto bit=1U<<static_cast<unsigned>((axis<0?-axis:axis)-1);
        if(used&bit)throw std::invalid_argument("Reference position frame axes must form a permutation");used|=bit;
    }
    for(float value:frame.parameters.offset)if(!std::isfinite(value))throw std::invalid_argument("Reference position frame offsets must be finite");
}
inline std::uint32_t referencePositionFrameCode(const ReferencePositionFrame& frame){
    validateReferencePositionFrame(frame);std::uint32_t result=0;
    for(unsigned i=0;i<3;++i){const auto axis=frame.parameters.axes[i];const auto code=static_cast<unsigned>((axis<0?-axis:axis)-1)|(axis<0?4U:0U);result|=code<<(i*3);}
    return result;
}
inline std::array<float,3> referenceRelativePosition(const ReferencePositionFrame& frame,const std::array<float,3>& world){
    validateReferencePositionFrame(frame);
    for(float value:world)if(!std::isfinite(value))throw std::invalid_argument("Reference world position must be finite");
    std::array<float,3> result;
    for(unsigned i=0;i<3;++i){const auto axis=frame.parameters.axes[i];const auto value=world[static_cast<unsigned>((axis<0?-axis:axis)-1)];
        volatile float rounded=(axis<0?-value:value)+frame.parameters.offset[i];result[i]=rounded;
        if(!std::isfinite(result[i]))throw std::invalid_argument("Reference position frame overflows binary32");}
    return result;
}
}
