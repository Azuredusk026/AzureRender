#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <type_traits>
namespace azurerender {
struct alignas(16) ReferenceClipProjectionParameters {
    using Row=std::array<float,4>;
    std::array<Row,4> columns{Row{1,0,0,0},Row{0,1,0,0},Row{0,0,1,0},Row{0,0,0,1}};
    Row jitter{};
};
static_assert(sizeof(ReferenceClipProjectionParameters)==80);
static_assert(std::is_standard_layout_v<ReferenceClipProjectionParameters>);
struct ReferenceClipProjectionProfile {
    std::uint32_t schemaVersion=1;
    std::array<std::uint32_t,3> reserved{};
    ReferenceClipProjectionParameters parameters;
};
inline void validateReferenceClipProjection(const ReferenceClipProjectionProfile& profile){
    if(profile.schemaVersion!=1)throw std::invalid_argument("Unsupported reference clip projection version");
    for(auto value:profile.reserved)if(value!=0)throw std::invalid_argument("Reference clip projection header reserves must be zero");
    for(const auto& column:profile.parameters.columns)for(float value:column)
        if(!std::isfinite(value))throw std::invalid_argument("Reference clip projection columns must be finite");
    const auto& jitter=profile.parameters.jitter;
    for(float value:jitter)if(!std::isfinite(value))throw std::invalid_argument("Reference clip projection jitter must be finite");
    if(std::abs(jitter[0])>1||std::abs(jitter[1])>1||jitter[2]!=0||jitter[3]!=0)
        throw std::invalid_argument("Reference clip projection requires normalized UV jitter and zero reserved fields");
}
inline std::array<float,4> projectReferenceClip(const ReferenceClipProjectionProfile& profile,const std::array<float,3>& relativePosition){
    validateReferenceClipProjection(profile);
    for(float value:relativePosition)if(!std::isfinite(value))throw std::invalid_argument("Reference clip position must be finite");
    const auto& p=profile.parameters;std::array<float,4> result;
    for(unsigned row=0;row<4;++row){
        // Each assignment is a binary32 boundary. Explicit FMA calls retain
        // their fused rounding while the separate product/add stay separate.
        volatile float value=relativePosition[1]*p.columns[1][row];
        value=std::fma(p.columns[0][row],relativePosition[0],value);
        value=std::fma(p.columns[2][row],relativePosition[2],value);
        result[row]=value+p.columns[3][row];
        if(!std::isfinite(result[row]))throw std::invalid_argument("Reference clip projection overflows binary32");
    }
    for(unsigned axis=0;axis<2;++axis){
        volatile float offset=result[3]*p.jitter[axis];
        result[axis]=std::fma(-offset,axis==0?2.F:-2.F,result[axis]);
        if(!std::isfinite(result[axis]))throw std::invalid_argument("Reference clip jitter overflows binary32");
    }
    return result;
}
}
