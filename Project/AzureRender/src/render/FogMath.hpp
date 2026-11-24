#pragma once
#include <cmath>
#include <limits>
#include <algorithm>
namespace azurerender::detail {
inline double fogLogExprel(double x){
    if(std::abs(x)<1e-5)return std::log1p(x*.5+x*x/6+x*x*x/24);
    if(x>50)return x-std::log(x)+std::log1p(-std::exp(-x));
    if(x< -50)return std::log1p(-std::exp(x))-std::log(-x);
    return std::log(std::expm1(x)/x);
}
inline double fogTransmittanceFromLogDepth(double value){
    return value>std::log(std::numeric_limits<double>::max())?0:std::exp(-std::exp(value));
}
inline double fogLogHeightAverage(double falloff,double reference,double start,double end){
    return -falloff*(std::min(start,end)-reference)+fogLogExprel(-falloff*std::abs(end-start));
}
}
