#include "render/AnalyticFog.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace azurerender {
namespace {void finite(float v){if(!std::isfinite(v))throw std::invalid_argument("Analytic fog requires finite parameters");}}
void LayeredHeightFog::validate() const {
    if(layers.empty()||layers.size()>2)throw std::invalid_argument("Height fog requires one or two layers");
    finite(startViewDepth);finite(minimumTransmittance);
    if(startViewDepth<0||minimumTransmittance<0||minimumTransmittance>1)throw std::invalid_argument("Invalid height fog distance or minimum transmittance");
    for(auto v:transmittanceBias)finite(v);
    for(const auto& layer:layers){
        for(auto v:{layer.density,layer.heightFalloff,layer.referenceHeight})finite(v);
        if(layer.density<0||layer.heightFalloff<0)throw std::invalid_argument("Height density and falloff must be nonnegative");
        for(auto v:layer.scatteringColor){finite(v);if(v<0)throw std::invalid_argument("Height scattering must be nonnegative");}
    }
}
void AnalyticFogSettings::validate() const {
    if(!aerial&&!height)throw std::invalid_argument("Analytic fog requires a medium");
    if(aerial)aerial->validate();if(height)height->validate();
}
FogTransport evaluateAnalyticFog(const AnalyticFogSettings& p,std::array<float,3> start,std::array<float,3> end,std::array<float,3> forward){
    p.validate();for(const auto& v:{start,end,forward})for(auto x:v)finite(x);
    const double norm=std::hypot(double(forward[0]),double(forward[1]),double(forward[2]));
    if(norm==0)throw std::invalid_argument("Analytic fog requires a view direction");
    double depth=0;for(unsigned i=0;i<3;++i)depth+=(double(end[i])-start[i])*forward[i]/norm;
    FogTransport height;
    if(p.height&&depth>p.height->startViewDepth){
        std::array<float,3> begin{};const double fraction=p.height->startViewDepth/depth;
        for(unsigned i=0;i<3;++i)begin[i]=static_cast<float>(start[i]+(double(end[i])-start[i])*fraction);
        for(const auto& layer:p.height->layers)height=composeFogTransport(height,evaluateHeightFog(layer,begin,end));
        const double distance=std::hypot(double(end[0])-start[0],double(end[1])-start[1],double(end[2])-start[2]);
        const auto& b=p.height->transmittanceBias;
        const double bias=std::clamp(double(b[0])+distance*b[1],0.0,1.0)+std::clamp(double(b[2])+distance*b[3],0.0,1.0);
        const double original=height.transmittance[0];
        const float adjusted=static_cast<float>(std::clamp(std::max(original,double(p.height->minimumTransmittance))+bias,0.0,1.0));
        if(adjusted!=original){
            const double scale=original<1?(1-adjusted)/(1-original):0;
            for(unsigned i=0;i<3;++i){height.transmittance[i]=adjusted;height.scattering[i]=static_cast<float>(height.scattering[i]*scale);}
        }
    }
    return composeFogTransport(height,p.aerial?evaluateAerialFog(*p.aerial,start,end):FogTransport{});
}
}
