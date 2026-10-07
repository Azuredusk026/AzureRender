#pragma once
#include "scene/TransformMath.hpp"
#include <cmath>
#include <stdexcept>
namespace azurerender::scene {
struct DecomposedTransform {
    internal::Vector3 translation{}, rotation{}, scale{1,1,1};
};
inline internal::Matrix4 inverseAffine(const internal::Matrix4& matrix) {
    for(const auto value:matrix)if(!std::isfinite(value))throw std::invalid_argument("Transform must be finite");
    if(std::abs(matrix[3])>1e-6F||std::abs(matrix[7])>1e-6F||std::abs(matrix[11])>1e-6F||std::abs(matrix[15]-1)>1e-6F)
        throw std::invalid_argument("Transform must be affine");
    double rows[4][8]{};
    for(unsigned row=0;row<4;++row)for(unsigned column=0;column<4;++column){rows[row][column]=matrix[column*4+row];rows[row][column+4]=row==column?1:0;}
    for(unsigned column=0;column<4;++column){
        unsigned pivot=column;for(unsigned row=column+1;row<4;++row)if(std::abs(rows[row][column])>std::abs(rows[pivot][column]))pivot=row;
        if(std::abs(rows[pivot][column])<1e-12)throw std::invalid_argument("Transform is singular");
        for(unsigned entry=0;entry<8;++entry)std::swap(rows[column][entry],rows[pivot][entry]);
        const double divisor=rows[column][column];for(auto& entry:rows[column])entry/=divisor;
        for(unsigned row=0;row<4;++row)if(row!=column){const auto factor=rows[row][column];for(unsigned entry=0;entry<8;++entry)rows[row][entry]-=factor*rows[column][entry];}
    }
    internal::Matrix4 inverse{};
    for(unsigned row=0;row<4;++row)for(unsigned column=0;column<4;++column){inverse[column*4+row]=static_cast<float>(rows[row][column+4]);if(!std::isfinite(inverse[column*4+row]))throw std::invalid_argument("Transform inverse exceeds range");}
    return inverse;
}
inline DecomposedTransform decomposeTrs(const internal::Matrix4& matrix,const internal::Vector3& preferredScale={1,1,1}) {
    (void)inverseAffine(matrix);
    DecomposedTransform result;result.translation={matrix[12],matrix[13],matrix[14]};
    internal::Vector3 columns[3];
    for(unsigned axis=0;axis<3;++axis){
        columns[axis]={matrix[axis*4],matrix[axis*4+1],matrix[axis*4+2]};
        result.scale[axis]=internal::vectorLength(columns[axis])*(preferredScale[axis]<0?-1.F:1.F);
        if(std::abs(result.scale[axis])<1e-7F)throw std::invalid_argument("Transform scale is too small");
        columns[axis]=internal::scaleVector(columns[axis],1/result.scale[axis]);
    }
    if(internal::dot(internal::cross(columns[0],columns[1]),columns[2])<0){result.scale[0]=-result.scale[0];columns[0]=internal::scaleVector(columns[0],-1);}
    for(unsigned a=0;a<3;++a)for(unsigned b=a+1;b<3;++b)
        if(std::abs(internal::dot(columns[a],columns[b]))>1e-4F)throw std::invalid_argument("Transform contains shear that local TRS cannot represent");
    constexpr float degrees=180.F/3.14159265358979323846F;
    const float y=std::asin(std::clamp(columns[2][0],-1.F,1.F));
    const float x=std::abs(std::cos(y))>1e-5F?std::atan2(columns[2][1],columns[2][2]):std::atan2(-columns[1][2],columns[1][1]);
    const float z=std::abs(std::cos(y))>1e-5F?std::atan2(columns[1][0],columns[0][0]):0;
    result.rotation={x*degrees,y*degrees,z*degrees};
    const auto reconstructed=composeTrs(result.translation,result.rotation,result.scale);
    for(unsigned i=0;i<16;++i)if(std::abs(matrix[i]-reconstructed[i])>2e-4F*std::max(1.F,std::abs(matrix[i])))
        throw std::invalid_argument("Transform decomposition exceeds precision budget");
    return result;
}
}
